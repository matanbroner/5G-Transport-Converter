// TCP Fast Open Server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/mptcp.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "../lib_convert/convert_util.h"
#include "net_headers.h"

#define CONVERT_HDR_LEN sizeof(struct convert_header)
#define MAX_PACKET_SIZE 1500

void parse_convert(char *buffer)
{
    uint8_t hdr[CONVERT_HDR_LEN];
    struct convert_opts *opts;
    int length;

    if (convert_parse_header(hdr, CONVERT_HDR_LEN, &length) < 0)
    {
        printf("unable to read the convert header\n");
        return;
    }

    if (length)
    {
        opts = convert_parse_tlvs(buffer + CONVERT_HDR_LEN, length - CONVERT_HDR_LEN);
        if (opts == NULL)
            return;

        /* if we receive the TLV error we need to inform the app */
        if (opts->flags & CONVERT_F_ERROR)
        {
            printf("received TLV error: %u\n", opts->error_code);
            convert_free_opts(opts);
        }
    }
}

int main()
{
    // Open a server socket and listen for connections
    // Server is an MPTCP socket
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0)
    {
        perror("socket() error: ");
        return 1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8081);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind() error: ");
        return 1;
    }

    printf("Server listening on IP: %s, Port: %d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));

    while (1)
    {
        char buf[MAX_PACKET_SIZE];
        struct sockaddr_in saddr;
        socklen_t saddr_len = sizeof(saddr);
        if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&saddr, &saddr_len) < 0)
        {
            perror("recvfrom() error: ");
            continue;
        }

        // Parse the TCP headers
        struct tcp_packet *tcp = (struct tcp_packet *)buf;
        struct tcp_header *tcp_hdr = &tcp->tcphdr;
        struct ip_header *ip_hdr = &tcp->iphdr;

        // filter packets to port 8081
        if (ntohs(tcp_hdr->th_dport) != 8081)
            continue;

        // Parse the Convert headers
        char *convert_hdr = tcp->data;
        parse_convert(convert_hdr);

        // Log the packet
        printf("Received packet from %s:%d to %s:%d\n", inet_ntoa(ip_hdr->ip_src), ntohs(tcp_hdr->th_sport), inet_ntoa(ip_hdr->ip_dst), ntohs(tcp_hdr->th_dport));
    }
    return 0;
}
