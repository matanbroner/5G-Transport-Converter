// Author: Matan Broner
// headers.c
// This file contains the headers for incoming and outgoing packets.
// These are used by the Transport Converter which needs
// to read and write packets through a raw socket.

#include <netinet/tcp.h>
#include <netinet/ip.h>


struct ip_header {
    unsigned char ip_hl:4;
    unsigned char ip_v:4;
    unsigned char ip_tos;
    unsigned short int ip_len;
    unsigned short int ip_id;
    unsigned short int ip_off;
    unsigned char ip_ttl;
    unsigned char ip_p;
    unsigned short int ip_sum;
    struct in_addr ip_src;
    struct in_addr ip_dst;
};

struct tcp_header {
    unsigned short int th_sport;
    unsigned short int th_dport;
    unsigned int th_seq;
    unsigned int th_ack;
    unsigned char th_x2:4;
    unsigned char th_off:4;
    unsigned char th_flags;
    unsigned short int th_win;
    unsigned short int th_sum;
    unsigned short int th_urp;
};


struct tcp_packet {
    struct ip_header iphdr;
    struct tcp_header tcphdr;
    char data[1500 - sizeof(struct iphdr) - sizeof(struct tcphdr)];
};
