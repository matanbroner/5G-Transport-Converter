#ifndef _TC_SERVER_H_
#define _TC_SERVER_H_

ogs_hash_t *proxy_hash;
ogs_hash_t *conn_hash;

// Proxy structure
typedef struct proxy_s {
    int fd;
    ip_t from_ip;
    ip_t to_ip;
    uint16_t from_port;
    uint16_t to_port;
} proxy_t;

// Utility methods
char *gen_proxy_key(ip_t from_ip, uint16_t from_port, ip_t to_ip, uint16_t to_port) {
    char* key = malloc(64 * sizeof(char));
    char* from_ip_str = inet_ntop(AF_INET, &proxy->from_ip, NULL, 0);
    char* to_ip_str = inet_ntop(AF_INET, &proxy->to_ip, NULL, 0);
    sprintf(key, "%s:%d,%s:%d", from_ip_str, proxy->from_port, to_ip_str, proxy->to_port);
}

// Infrastructure methods
int init_tc_server(void);
int terminate_tc_server(void);

// Proxy methods
int handle_syn(void * data, int len);

#endif /* _TC_SERVER_H_ */