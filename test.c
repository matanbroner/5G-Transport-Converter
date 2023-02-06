#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/mptcp.h>

int main(int argc, char **argv)
{
    // Open a socket to localhost and read from it
    // Use the socket() function from the library
    // The socket() function will print a message

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.56.107", &serv_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    close(sockfd);
    return 0;
}