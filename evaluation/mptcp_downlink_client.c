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
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 100000
#define REPORT_INTERVAL 1

// Colors
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[34m"
#define NORMAL "\x1b[0m"

int BYTES_READ = 0;
int LOOP = 1;

void log_color(char *color, char *msg)
{
    printf("[MPTCP Downlink Client] %s%s%s\n", color, msg, NORMAL);
}

// To be run in a separate thread
void* log_bytes_read(){
    // Every 1 second, print the number of bytes read
    int last_read = 0;
    while (1) {
        sleep(REPORT_INTERVAL);
        char *msg = malloc(100);
        sprintf(msg, "Read %d bytes from server in last %d seconds", (int)(BYTES_READ - last_read), REPORT_INTERVAL);
        log_color(NORMAL, msg);
        free(msg);
        last_read = BYTES_READ;
    }
}

// On keyboard interrupt, kill the loop
void sigint_handler(int sig)
{
    char* msg = malloc(100);
    sprintf(msg, "Keyboard interrupt signal %d received, exiting...", sig);
    log_color(BLUE, msg);
    free(msg);
    LOOP = 0;
}

int main(int argc, char **argv)
{
    // Args should be the local IP, server IP, server port
    if (argc < 4) {
        printf("Usage: %s <local IP> <server IP> <server port> [buffer size]\n", argv[0]);
        return -1;
    }
    char *local_ip = argv[1];
    char *server_ip = argv[2];
    int server_port = atoi(argv[3]);
    int buffer_size = BUFFER_SIZE;
    if (argc == 5) {
        buffer_size = atoi(argv[4]);
    }

    log_color(BLUE, "Starting client...");
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);

    if (sockfd < 0) {
        log_color(RED, "socket() failed");
        return -1;
    }

    // Bind to given IP and port
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(local_ip); // Specify the source IP address here
    	
    if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        log_color(RED, "bind() failed");
        exit(1);
    } 


    // Connect to server
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;

    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    serv_addr.sin_port = htons(server_port);
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_color(RED, "connect() failed");
        return -1;
    } else {
        char *msg = malloc(100);
        sprintf(msg, "Connected to server %s:%d", server_ip, server_port);
        log_color(GREEN, msg);
        free(msg);
    }

    // Start a thread to log the number of bytes read
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, log_bytes_read, NULL);

    // Set up signal handler for keyboard interrupt
    signal(SIGINT, sigint_handler);

    while (LOOP) {
        char buffer[buffer_size];
        int n = read(sockfd, buffer, buffer_size);
        if (n <= 0) {
            log_color(RED, "Error reading data from server...");
            break;
        } else {
            BYTES_READ += n;
	    }
    }
    log_color(BLUE, "Closing connection...");
    close(sockfd);
    pthread_join(thread_id, NULL);
    return 0;
}
