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

    printf("Starting client test code...\n");
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    // print socket src ip and port and dst ip and port
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getsockname(sockfd, (struct sockaddr *)&addr, &addr_size);
    printf("Socket src %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
    printf("Socket dst %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    // Read from the socket while data is available
    char buffer[1024];
    int n;
    while ((n = read(sockfd, buffer, sizeof(buffer))) > 0) {
        printf("Received %d bytes: %s\n", n, buffer);
    }
    printf("Closing socket...\n");
    close(sockfd);
    return 0;
}