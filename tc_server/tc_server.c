#include "./tc_server.h"
#include "convert_util.h"

// Initialization function
// Create the OGS Hash Map to hold the connections
// that are being proxied
int init_tc_server()
{
    proxy_hash = ogs_hash_make();
    if (proxy_hash == NULL)
    {
        return -1;
    }
    conn_hash = ogs_hash_make();
    if (conn_hash == NULL)
    {
        return -1;
    }
    return 0;
}

// Terminates the server
// Closes all the connections
// Destroys the OGS Hash Map
int terminate_tc_server()
{
    ogs_hash_destroy(proxy_hash);
    return 0;
}

// Handle incoming packets
// Create a new connection and add it to the OGS Hash Map
// Creates a new socket and connects to the destination
// using Convert Protocol
int handle_packet(void *data, int len, char *err, int err_buf_len)
{
    struct convert_opts *opts;
    size_t tlv_len;

    // IP Header
    struct iphdr *iph = (struct iphdr *)data;

    // Fail if protocol is not MP_TCP
    if (iph->protocol != IPPROTO_MPTCP)
    {
        snprintf(err, err_buf_len, "Protocol is not MP_TCP");
        return -1;
    }

    // TCP Ports
    struct tcphdr *tcph = (struct tcphdr *)(data + iph->ihl * 4);

    char *proxy_key = gen_proxy_key(iph->saddr, tcph->src_port, iph->daddr, tcph->dst_port);
    if (proxy_key == NULL)
    {
        snprintf(err, err_buf_len, "Failed to generate connection key");
        return -1;
    }

    // Check if connection already exists
    struct proxy_t *proxied = ogs_hash_get(proxy_hash, proxy_key, strlen(conn_key));
    if (proxied != NULL)
    {
        // TODO: Handle send data as is to the proxied connection
    }

    free(proxy_key);

    // Remaining data
    data = data + iph->ihl * 4 + tcph->doff * 4;
    len = len - iph->ihl * 4 - tcph->doff * 4;

    // Parse the Convert header
    if (convert_parse_header(data, len, &tlv_len) < 0)
    {
        snprintf(err, err_buf_len, "Failed to parse Convert header");
        return -1;
    }
    // Handle the Convert TLVs
    opts = convert_parse_tlv(data, tlv_len);
    if (opts == NULL)
    {
        snprintf(err, err_buf_len, "Failed to parse Convert TLVs");
        return -1;
    }

    // If we got a TLV error flag, return an error
    if (opts->flags & CONVERT_F_ERROR)
    {
        snprintf(err, err_buf_len, "Convert TLV error flag set");
        return -1;
    }

    // Handle the Convert TLVs
    if (opts->flags & CONVERT_F_CONNECT)
    {
        // Handle TLV Connect
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            return -1;
        }
        if (connect(sock, (struct sockaddr *)&opts->remote_addr, sizeof(struct sockaddr_in)) < 0)
        {
            return -1;
        }
        // Create the proxy connection object
        struct proxy_t *proxy = get_new_proxy_obj();

        proxy->fd = sock;
        proxy->from_ip = iph->saddr;
        proxy->from_port = tcph->sport;
        proxy->to_ip = iph->daddr;
        proxy->to_port = tcph->dport;

        proxy->fd = sock;
        proxy->from_addr = opts->remote_addr;

        // Add the connection to the OGS Hash Map
        char* proxy_key = gen_proxy_key(iph->saddr, tcph->src_port, iph->daddr, tcph->dst_port);
        if (proxy_key == NULL)
        {
            snprintf(err, err_buf_len, "Failed to generate connection key");
            return -1;
        }
        ogs_hash_set(proxy_hash, proxy_key, strlen(proxy_key), proxy);
    }

    return 0;
}
