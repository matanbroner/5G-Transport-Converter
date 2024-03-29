// MPTCP Client
// This client will connect to the server and send/receive data
// It can operate in 3 modes:
// 1. Uplink client: send data to the server and never read
// 2. Downlink client: read data from the server and never send
// 3. Echo client: send data to the server and read the response (default)
//
// Usage: ./mptcp_client <local IP> <server IP> <server port> [buffer size] [--uplink | --downlink]

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
#include <time.h>
#include <sys/time.h>

#define BUFFER_SIZE 100000
#define REPORT_INTERVAL 1

// Colors
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define BLUE "\x1b[34m"
#define NORMAL "\x1b[0m"

#define UPLINK_CLIENT 0
#define DOWNLINK_CLIENT 1
#define ECHO_CLIENT 2


int MAGIC_NUMBER = 0x7F;
int BYTES_READ = 0;
int BYTES_WRITTEN = 0;
int LOOP = 1;
int CLIENT_TYPE = ECHO_CLIENT;
int DOWNLOAD_SIZE = -1;
int UPLOAD_SIZE = -1;

void log_color(char *color, char *msg)
{
    char* client_type = malloc(20);
    if (CLIENT_TYPE == UPLINK_CLIENT) {
        client_type = "Uplink";
    } else if (CLIENT_TYPE == DOWNLINK_CLIENT) {
        client_type = "Downlink";
    } else {
        client_type = "Echo";
    }
    printf("[MPTCP %s Client] %s%s%s\n", client_type, color, msg, NORMAL);
}

// To be run in a separate thread
void* log_metrics(){
    // Every 1 second, print the number of bytes read/written
    int last_read = 0;
    int last_written = 0;
    while (LOOP) {
        sleep(REPORT_INTERVAL);
        char* msg = malloc(100);
        if (CLIENT_TYPE == DOWNLINK_CLIENT || CLIENT_TYPE == ECHO_CLIENT) {
            // zero out the buffer
            memset(msg, 0, 100);
            sprintf(msg, "Read %d bytes from server in last %d seconds", (int)(BYTES_READ - last_read), REPORT_INTERVAL);
            log_color(NORMAL, msg);
            last_read = BYTES_READ;
        }
        if (CLIENT_TYPE == UPLINK_CLIENT || CLIENT_TYPE == ECHO_CLIENT) {
            // zero out the buffer
            memset(msg, 0, 100);
            sprintf(msg, "Wrote %d bytes to server in last %d seconds", (int)(BYTES_WRITTEN - last_written), REPORT_INTERVAL);
            log_color(NORMAL, msg);
            last_written = BYTES_WRITTEN;
        }
        free(msg);
    }
    return NULL;
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
        printf("Usage: %s <local IP> <server IP> <server port> [buffer size] [--uplink | --downlink] [--download-size <bytes>] [--upload-size <bytes>]\n", argv[0]);
        printf("\t* local IP: the IP address of the interface to bind to\n");
        printf("\t* server IP: the IP address of the server to connect to\n");
        printf("\t* server port: the port of the server to connect to\n");
        printf("\t* buffer size: the size of the buffer to use for reading/writing\n");
        printf("\t* --download-size: the number of bytes to download from the server\n");
        printf("\t* --upload-size: the number of bytes to upload to the server\n");
        printf("\t* --uplink: run as uplink client, send uplnik data and never read\n");
        printf("\t* --downlink: run as downlink client, read from server and never send\n");
        return -1;
    }
    char *local_ip = argv[1];
    char *server_ip = argv[2];
    int server_port = atoi(argv[3]);
    int buffer_size = BUFFER_SIZE;
    if (argc >= 5) {
        for (int i = 4; i < argc; i++)
        {
            // if is client type flag, otherwise is buffer size
            // do not make sure we only have one of each, just override values as we go
            if (strcmp(argv[i], "--uplink") == 0) {
                CLIENT_TYPE = UPLINK_CLIENT;
            } else if (strcmp(argv[i], "--downlink") == 0) {
                CLIENT_TYPE = DOWNLINK_CLIENT;
            } else if (strcmp(argv[i], "--download-size") == 0) {
                DOWNLOAD_SIZE = atoi(argv[i+1]);
                i++;
            } else if (strcmp(argv[i], "--upload-size") == 0) {
                UPLOAD_SIZE = atoi(argv[i+1]);
                i++;
            }  else {
                buffer_size = atoi(argv[i]);
            }
        }
        
    }
    log_color(BLUE, "Starting client...");
    char* msg = malloc(100);
    sprintf(msg, "Local IP: %s, Server IP: %s, Server Port: %d, Buffer Size: %d", local_ip, server_ip, server_port, buffer_size);
    log_color(BLUE, msg);
    free(msg);
    char* msg2 = malloc(100);
    sprintf(msg2, "Client type: %d, Download size: %d, Upload size: %d", CLIENT_TYPE, DOWNLOAD_SIZE, UPLOAD_SIZE);
    free(msg2);

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
    pthread_create(&thread_id, NULL, log_metrics, NULL);

    // Set up signal handler for keyboard interrupt
    signal(SIGINT, sigint_handler);

    // Send the server the magic number so it knows we're an MPTCP client
    char *magic = malloc(1);
    memset(magic, 0, 1);
    memcpy(magic, &MAGIC_NUMBER, 1);
    if (write(sockfd, magic, 1) < 0) {
        log_color(RED, "write() failed for magic number");
        return -1;
    }
    free(magic);

    // Send the server a 1 byte message to tell it what type of client we are
    char *client_type = malloc(1);
    memset(client_type, 0, 1);
    client_type[0] = CLIENT_TYPE;
    if (write(sockfd, client_type, 1) < 0) {
        log_color(RED, "write() failed for client type");
        return -1;
    }
    free(client_type);

    // Send the server the buffer size we're using
    char *buffer_size_str = malloc(4);
    memset(buffer_size_str, 0, 4);
    memcpy(buffer_size_str, &buffer_size, 4);
    if (write(sockfd, buffer_size_str, 4) < 0) {
        log_color(RED, "write() failed for buffer size");
        return -1;
    }
    free(buffer_size_str);

    char* buffer = malloc(buffer_size);
    // Start clock to measure time to read magic number (if downlink/echo)
    struct timeval start, end;
    gettimeofday(&start, NULL); // capture the start time

    while (LOOP) {
        memset(buffer, 0, buffer_size);
        // if downlink client or echo client, read from server
        if (CLIENT_TYPE == DOWNLINK_CLIENT || CLIENT_TYPE == ECHO_CLIENT) {
            int bytes_read = read(sockfd, buffer, buffer_size);
            if (bytes_read < 0) {
                log_color(RED, "read() failed");
                return -1;
            }

            // if first two bytes are magic number, stop clock
            if (buffer[0] == MAGIC_NUMBER && (CLIENT_TYPE == DOWNLINK_CLIENT || CLIENT_TYPE == ECHO_CLIENT)) {
                gettimeofday(&end, NULL);
                double elapsed = (double)(end.tv_sec - start.tv_sec) * 1000.0;
                elapsed += (double)(end.tv_usec - start.tv_usec) / 1000.0;

                log_color(GREEN, "Received magic number from server");
                char* msg = malloc(100);
                sprintf(msg, "Time to receive magic number: %f", elapsed);
                log_color(GREEN, msg);
                free(msg);
                // reset buffer and continue
                memset(buffer, 0, buffer_size);
                continue;
            }
            BYTES_READ += bytes_read;
            if (DOWNLOAD_SIZE != -1 && BYTES_READ >= DOWNLOAD_SIZE) {
                gettimeofday(&end, NULL);
                double elapsed = (double)(end.tv_sec - start.tv_sec) * 1000.0;
                elapsed += (double)(end.tv_usec - start.tv_usec) / 1000.0;

                log_color(GREEN, "Completed download");
                char* msg = malloc(100);
                sprintf(msg, "Time to download %d bytes: %f", BYTES_READ, elapsed);
                log_color(GREEN, msg);
                free(msg);
                break;
            }
            if (CLIENT_TYPE == DOWNLINK_CLIENT) {
                // if downlink client, do not write back
                // reset buffer and continue
                memset(buffer, 0, buffer_size);
                continue;
            }
        }
        // if uplink client or echo client, write to server
        if (CLIENT_TYPE == UPLINK_CLIENT || CLIENT_TYPE == ECHO_CLIENT) {
            // if not echo client, write random data
            if (CLIENT_TYPE != ECHO_CLIENT) {
                for (int i = 0; i < buffer_size; i++)
                {
                    buffer[i] = rand() % 256;
                }
            }
            int bytes_written = write(sockfd, buffer, buffer_size);
            if (bytes_written < 0) {
                log_color(RED, "write() failed");
                return -1;
            }
            BYTES_WRITTEN += bytes_written;
            if (UPLOAD_SIZE != -1 && BYTES_WRITTEN >= UPLOAD_SIZE) {
                gettimeofday(&end, NULL);
                double elapsed = (double)(end.tv_sec - start.tv_sec) * 1000.0;
                elapsed += (double)(end.tv_usec - start.tv_usec) / 1000.0;

                log_color(GREEN, "Completed upload");
                char* msg = malloc(100);
                sprintf(msg, "Time to upload %d bytes: %f", BYTES_WRITTEN, elapsed);
                log_color(GREEN, msg);
                free(msg);
                break;
            }
            memset(buffer, 0, buffer_size);
        }
    }
    free(buffer);
    log_color(BLUE, "Closing connection...");
    LOOP = 0;
    close(sockfd);
    pthread_join(thread_id, NULL);
    return 0;
}
