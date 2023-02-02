#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <linux/mptcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "./lib_convert/convert.h"
#include "./lib_convert/convert_util.h"

// TODO: improve the map to use a linked list
#include "map.h"

/**
 * socket() intercept
 * Intercept socket() calls created in user code
 * If socket is MPTCP (ie. protocol == IPPROTO_MPTCP), create the socket ourselves
 * and store it in a map so we know when it connects
 * Otherwise, just call the original socket() function
 * @param domain
 * @param type
 * @param protocol
 * @return
*/
int socket(int domain, int type, int protocol)
{
    // get the original socket() function
    int (*lsocket)(int, int, int) = dlsym(RTLD_NEXT, "socket");
    printf("socket() called with domain=%d, type=%d, protocol=%d\n", domain, type, protocol);
    if (protocol == IPPROTO_MPTCP)
    {
        // if an MPTCP socket, create the fd ourselves and
        // store it so we know when it connects
        int fd = lsocket(domain, type, protocol);
        // set empty string as value for now
        // TODO: change this to a struct with IP field set to NULL
        map_set(fd, "");
        return fd;
    }
    // otherwise, just call the original socket() function
    return lsocket(domain, type, protocol);
}

/**
 * connect() intercept
 * Intercept connect() calls created in user code
 * If socket is MPTCP (ie. we have it in the map), use TCP fast open and the Convert Protocol
 * to send the IP address to the Transport Converter and connect to it
 * Otherwise, just call the original connect() function
 * @param sockfd
 * @param addr
 * @param addrlen
 * @return
*/
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // get the original connect() function
    int (*lconnect)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "connect");
    printf("connect() called with sockfd=%d, addr=%p, addrlen=%d\n", sockfd, addr, addrlen);

    // check if the socket is MPTCP (ie. in the map)
    if (map_get(sockfd) != NULL)
    {
        // store the IP address actually being connected to
        // mapped to its socket fd
        char *ip = malloc(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(((struct sockaddr_in *)addr)->sin_addr), ip, INET_ADDRSTRLEN);
        map_set(sockfd, ip);

        // Generate the Convert Protocol packet payload
        uint8_t buf[1024];
	    ssize_t len;

        // use TCP fast open to include the IP address in the SYN packet payload
        char* data = ip;
        int data_len = strlen(ip);
        // set the IP to the Transport Converter IP
        inet_pton(AF_INET, "192.168.56.107", &(((struct sockaddr_in *)addr)->sin_addr));
        // set the port to the Transport Converter port
        ((struct sockaddr_in *)addr)->sin_port = htons(8080);
        if(sendto(sockfd, data, data_len, MSG_FASTOPEN, addr, addrlen) < 0)
        {
            perror("sendto() failed");
            return -1;
        }
        return 0;
    }
    return lconnect(sockfd, addr, addrlen);
}

// Library constructor
__attribute__((constructor)) void init(void)
{
    printf("Library loaded\n");
    map_init();
}

// Library destructor
__attribute__((destructor)) void fini(void)
{
    printf("Library unloaded\n");
    map_free();
}