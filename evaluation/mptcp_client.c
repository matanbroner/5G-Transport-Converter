#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/mptcp.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>

#define BUFFER_SIZE 100000

int main(int argc, char **argv)
{
    // Open a socket to localhost and read from it
    // Use the socket() function from the library
    // The socket() function will print a message

    printf("Starting client test code...\n");
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);

    if (sockfd < 0) {
        printf("socket failed\n");
        return -1;
    } else {
        printf("socket success\n");
    }

    // Connect to "uesimtun0" interface
    // Bind the socket to a specific local IP address
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr("10.45.0.3"); // Specify the source IP address here
							//
    if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        perror("bind");
        exit(1);
    } else {
        printf("bind success\n");
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;

    // Connect to Google's DNS server and read from it
    
    // struct hostent *h;
    // h = gethostbyname("www.google.com");
    // if (h == NULL) {
    //    printf("gethostbyname failed\n");
    //    return -1;
    // } else {
    //    printf("gethostbyname success\n");
    // }

    // serv_addr.sin_addr.s_addr = *(unsigned long *)h->h_addr_list[0];
    serv_addr.sin_addr.s_addr = inet_addr("54.86.73.121");
    serv_addr.sin_port = htons(6000);
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("connect failed\n");
        return -1;
    } else {
        printf("connect success\n");
    }

    // print socket src ip and port and dst ip and port
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getsockname(sockfd, (struct sockaddr *)&addr, &addr_size);
    printf("Socket src %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
    printf("Socket dst %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    int sent = 0;
    int recvd = 0;
    while (1) {
	// printf("Entering loop...\n");
        // Receive data from the socket
        char buffer[BUFFER_SIZE];
        int n = read(sockfd, buffer, BUFFER_SIZE);
        if (n <= 0) {
            // Socket closed or error occurred
	    printf("Error reading from server...\n");
            break;
        } else {
	    // printf("Read %d bytes from server.\n", n);
	}


        // Mirror the data back to the client
        int bytes_sent = send(sockfd, buffer, n, 0);
        if (bytes_sent < 0) {
            printf("Error sending data to server...\n");
            break;
        } else {
		// printf("Sent %d bytes to server.\n");
	}
    }
    printf("Closing socket...\n");
    close(sockfd);
    return 0;
}
