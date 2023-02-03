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
        int err_code = 0;
        char error_msg[256];
        bool peek = false;
        struct convert_opts *opts = read_convert_opts(connfd, peek, &err_code, error_msg);

        // Check for errors
        if (err_code != 0)
        {
            printf("Error: [%d] %s\n", err_code, error_msg);
        }
        else
        {
            // Check the returned TLVs
            if(opts != NULL)
            {
            }
        }

        // Close the connection
        close(connfd);
    }
}
