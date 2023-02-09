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
#include <pthread.h>

#include "../lib_convert/convert_util.h"
#include "../lib_convert/bsd_queue.h"

/* State for each TCP connection proxy */
typedef struct socket_state
{
    LIST_ENTRY(socket_state)
    list;
    int c_fd; /* Client socket */
    int s_fd; /* Server socket */
} socket_state_t;

typedef struct thread_args
{
    int fd;
    bool is_client;
} thread_args_t;

// Hash table for socket state
#define NUM_BUCKETS 1024
static LIST_HEAD(socket_htbl_t, socket_state) _socket_htable[NUM_BUCKETS];
/* note: global hash table mutex, improvement: lock per bucket */
static pthread_mutex_t _socket_htable_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hash function
static unsigned int _socket_hash(int fd)
{
    return (unsigned int)fd % NUM_BUCKETS;
}

// Find socket state in hash table
static socket_state_t *_socket_find(int fd, bool is_client)
{
    unsigned int bucket = _socket_hash(fd);
    socket_state_t *state;
    pthread_mutex_lock(&_socket_htable_mutex);
    LIST_FOREACH(state, &_socket_htable[bucket], list)
    {
        if (state->c_fd == fd && is_client)
        {
            break;
        }
        else if (state->s_fd == fd && !is_client)
        {
            break;
        }
    }
    pthread_mutex_unlock(&_socket_htable_mutex);
    return state;
}

// Proxy data from one socket to another
// Keep proxying while both sockets are open
static int _proxy_data(void* args)
{
    thread_args_t *thread_args = (thread_args_t *)args;
    int fd = thread_args->fd;
    bool is_client = thread_args->is_client;

    printf("Starting proxy thread for fd %d (is_client=%d)\n", fd, is_client);

    while (1)
    {
        // Find the socket state for this socket
        socket_state_t *state = _socket_find(fd, is_client);
        if (!state)
        {
            return 1;
        }
        // Determine which socket to read from and which to write to
        int read_fd = is_client ? state->c_fd : state->s_fd;
        int write_fd = is_client ? state->s_fd : state->c_fd;
        // Read data from the socket
        char buf[4096];
        int n = read(read_fd, buf, sizeof(buf));
        if (n < 0)
        {
            perror("read() error: ");
            return 1;
        }
        else if (n == 0)
        {
            // Socket closed
            return 0;
        }
        // Write data to the socket
        if (write(write_fd, buf, n) < 0)
        {
            perror("write() error: ");
            return 1;
        }
    }
}


// Add socket state to hash table
static void _socket_add(socket_state_t *state)
{
    // We add to the table twice, once for the client and once for the server
    // This is because we need to be able to find the state for both the client
    // and server sockets depending on which direction the data is flowing

    // Client socket
    unsigned int bucket = _socket_hash(state->c_fd);
    pthread_mutex_lock(&_socket_htable_mutex);
    LIST_INSERT_HEAD(&_socket_htable[bucket], state, list);
    pthread_mutex_unlock(&_socket_htable_mutex);

    // Server socket
    bucket = _socket_hash(state->s_fd);
    pthread_mutex_lock(&_socket_htable_mutex);
    LIST_INSERT_HEAD(&_socket_htable[bucket], state, list);
    pthread_mutex_unlock(&_socket_htable_mutex);
}

// Remove socket state from hash table
static void _socket_remove(int fd, bool is_client)
{
    socket_state_t *state = _socket_find(fd, is_client);
    if (state)
    {
        pthread_mutex_lock(&_socket_htable_mutex);
        LIST_REMOVE(state, list);
        pthread_mutex_unlock(&_socket_htable_mutex);
        free(state);
    }
}

// Handle CONNECT_TLVs
static int _handle_connect_tlv(int fd, struct convert_opts *opts)
{
    struct sockaddr_in6 *remote_addr = &opts->remote_addr;
    // Create a socket to connect to the remote server
    int s_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (s_fd < 0)
    {
        perror("socket() error: ");
        return 1;
    }
    if (connect(s_fd, (struct sockaddr *)remote_addr, sizeof(struct sockaddr_in6)) < 0)
    {
        perror("connect() error: ");
        return 1;
    }
    // Create a new socket state
    socket_state_t *state = malloc(sizeof(socket_state_t));
    state->c_fd = fd;
    state->s_fd = s_fd;
    // Add the socket state to the hash table
    _socket_add(state);
    // Start two threads to proxy data in both directions
    thread_args_t c_args = {fd, true};
    thread_args_t s_args = {s_fd, false};
    pthread_t c_thread, s_thread;
    pthread_create(&c_thread, NULL, (void *)_proxy_data, (void *)&c_args);
    pthread_create(&s_thread, NULL, (void *)_proxy_data, (void *)&s_args);
    // Wait for the threads to finish
    pthread_join(c_thread, NULL);
    pthread_join(s_thread, NULL);
    // Remove the socket state from the hash table
    _socket_remove(fd, true);
    return 0;
}

// Start Function
int start(int port)
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
    serv_addr.sin_port = htons(port);
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
        // Use TCP Fast Open to send Convert headers to the client
        uint8_t buf[1024];
        struct convert_opts nil_opts = {0};
        int length = convert_write(buf, sizeof(buf), &nil_opts);
        if (send(connfd, buf, length, 0) < 0)
        {
            perror("send() error: ");
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
            if (opts != NULL)
            {
                // Check for the CONNECT_TLV
                if (opts->flags & CONVERT_F_CONNECT)
                {
                    // Handle the CONNECT_TLV
                    if (_handle_connect_tlv(connfd, opts) != 0)
                    {
                        printf("Error: Failed to handle CONNECT_TLV\n");
                        // Close the client socket
                        close(connfd);
                    }
                    write(connfd, "HTTP/1.1 200 OK\r", 16);
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]);
    start(port);
    return 0;
}
