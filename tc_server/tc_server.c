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

void read_convert(int sockfd, bool peek)
{
    uint8_t hdr[CONVERT_HDR_LEN];
    int ret;
    int flag = peek ? MSG_PEEK : 0;
    size_t length;
    size_t offset = peek ? CONVERT_HDR_LEN : 0;
    struct convert_opts *opts;

    printf("peek fd %d to see whether data is in the receive "
           "queue\n",
           sockfd);

    ret = recvfrom(sockfd, hdr, CONVERT_HDR_LEN, MSG_WAITALL | flag, NULL, NULL);

    printf("peek returned %d\n", ret);
    if (ret < 0)
    {
        return;
    }

    if (convert_parse_header(hdr, ret, &length) < 0)
    {
        printf("[%d] unable to read the convert header\n",
               sockfd);
        goto error;
    }

    if (length)
    {
        uint8_t buffer[length + offset];

        /* if peek the data was not yet read, so we need to
         * also read (again the main header). */
        if (peek)
            length += CONVERT_HDR_LEN;

        ret = recvfrom(sockfd, buffer, length, MSG_WAITALL, NULL, NULL);
        if (ret != (int)length || ret < 0)
        {
            printf("[%d] unable to read the convert"
                   " tlvs\n",
                   sockfd);
            goto error;
        }

        opts = convert_parse_tlvs(buffer + offset, length - offset);
        if (opts == NULL)
            goto error;

        /* if we receive the TLV error we need to inform the app */
        if (opts->flags & CONVERT_F_ERROR)
        {
            printf("received TLV error: %u\n", opts->error_code);
            convert_free_opts(opts);
        }
    }

error:
    /* ensure that a RST will be sent to the converter. */
    struct linger linger = {1, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
}

int main()
{
    // Open a server socket and listen for connections
    // Server is an MPTCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
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

    // Accept a connection
    if (listen(sockfd, 5) < 0)
    {
        perror("listen() error: ");
        return 1;
    }

    printf("Server listening on IP: %s, Port: %d\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));

    while (1)
    {
        // Accept a connection
        int connfd = accept(sockfd, (struct sockaddr *)NULL, NULL);
        if (connfd < 0)
        {
            perror("accept() error: ");
            return 1;
        }
        // Read the Convert headers and TLVs from the client (readr)
        read_convert(connfd, false);

        return 0;
    }
}
