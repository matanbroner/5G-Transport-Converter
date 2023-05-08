#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <linux/mptcp.h>
#include <linux/tcp.h>
#include <arpa/inet.h>

// Get MPTCP connection information for a given socket file descriptor
static PyObject *mptcp_util_get_mptcp_info(PyObject *self, PyObject *args)
{
    int sockfd;
    if (!PyArg_ParseTuple(args, "i", &sockfd))
        return NULL;

    struct mptcp_info mptcp_info;
    memset(&mptcp_info, 0, sizeof(mptcp_info));

    socklen_t len = sizeof(mptcp_info);
    // Get sock opt for mptcp info
    int ret = getsockopt(sockfd, SOL_MPTCP, MPTCP_INFO, &mptcp_info, &len);
    if (ret < 0)
    {
        int err = errno;
        char *err_msg = strerror(err);
        char *err_msg_full = malloc(strlen(err_msg) + 100);
        sprintf(err_msg_full, "Error getting mptcp info: [%d] %s", err, err_msg);
        PyErr_SetString(PyExc_RuntimeError, err_msg_full);
        free(err_msg_full);
        return NULL;
    }

    // Create a dictionary to hold the mptcp info
    PyObject *mptcp_dict = PyDict_New();
    
    // Populate the dictionary with the mptcp info
    PyDict_SetItemString(mptcp_dict, "mptcpi_subflows", PyLong_FromLong(mptcp_info.mptcpi_subflows));
    PyDict_SetItemString(mptcp_dict, "mptcpi_add_addr_signal", PyLong_FromLong(mptcp_info.mptcpi_add_addr_signal));
    PyDict_SetItemString(mptcp_dict, "mptcpi_add_addr_accepted", PyLong_FromLong(mptcp_info.mptcpi_add_addr_accepted));
    PyDict_SetItemString(mptcp_dict, "mptcpi_subflows_max", PyLong_FromLong(mptcp_info.mptcpi_subflows_max));
    PyDict_SetItemString(mptcp_dict, "mptcpi_add_addr_signal_max", PyLong_FromLong(mptcp_info.mptcpi_add_addr_signal_max));
    PyDict_SetItemString(mptcp_dict, "mptcpi_add_addr_accepted_max", PyLong_FromLong(mptcp_info.mptcpi_add_addr_accepted_max));
    PyDict_SetItemString(mptcp_dict, "mptcpi_flags", PyLong_FromLong(mptcp_info.mptcpi_flags));
    PyDict_SetItemString(mptcp_dict, "mptcpi_token", PyLong_FromLong(mptcp_info.mptcpi_token));
    PyDict_SetItemString(mptcp_dict, "mptcpi_write_seq", PyLong_FromLong(mptcp_info.mptcpi_write_seq));
    PyDict_SetItemString(mptcp_dict, "mptcpi_snd_una", PyLong_FromLong(mptcp_info.mptcpi_snd_una));
    PyDict_SetItemString(mptcp_dict, "mptcpi_rcv_nxt", PyLong_FromLong(mptcp_info.mptcpi_rcv_nxt));
    PyDict_SetItemString(mptcp_dict, "mptcpi_local_addr_used", PyLong_FromLong(mptcp_info.mptcpi_local_addr_used));
    PyDict_SetItemString(mptcp_dict, "mptcpi_local_addr_max", PyLong_FromLong(mptcp_info.mptcpi_local_addr_max));
    PyDict_SetItemString(mptcp_dict, "mptcpi_csum_enabled", PyLong_FromLong(mptcp_info.mptcpi_csum_enabled));

    // Return the dictionary of mptcp info
    return mptcp_dict;
}

// Get subflow information for a given socket file descriptor
static PyObject *mptcp_util_get_subflow_info(PyObject *self, PyObject *args)
{
    int sockfd;
    if (!PyArg_ParseTuple(args, "i", &sockfd))
        return NULL;

    struct subflow_info_t
    {
        struct mptcp_subflow_data d;
        struct mptcp_subflow_addrs addr[2];
    } subflow_info;

    memset(&subflow_info, 0, sizeof(subflow_info));

    subflow_info.d.size_subflow_data = sizeof(struct mptcp_subflow_data);
    subflow_info.d.size_user = sizeof(struct mptcp_subflow_addrs);

    socklen_t len = sizeof(subflow_info);
    // Get sock opt for subflow info
    int ret = getsockopt(sockfd, SOL_MPTCP, MPTCP_SUBFLOW_ADDRS, &subflow_info, &len);
    if (ret < 0)
    {
        int err = errno;
        char *err_msg = strerror(err);
        char *err_msg_full = malloc(strlen(err_msg) + 100);
        sprintf(err_msg_full, "Error getting subflow info: [%d] %s", err, err_msg);
        PyErr_SetString(PyExc_RuntimeError, err_msg_full);
        free(err_msg_full);
        return NULL;
    }
    // Create a list to hold all subflows as individual dictionaries
    int num_subflows = (int)(subflow_info.d.num_subflows);
    PyObject *subflow_list = PyList_New(num_subflows);
    // Iterate through each subflow and add it to the list
    for (int i = 0; i < num_subflows; i++)
    {
        PyObject *subflow_dict = PyDict_New();
        // Add the subflow id (we make up this id, it's not a real attribute)
        PyDict_SetItemString(subflow_dict, "id", PyLong_FromLong(i));

        // Get the addresses
        struct sockaddr *local = (struct sockaddr *)&subflow_info.addr[i].sa_local;
        struct sockaddr *remote = (struct sockaddr *)&subflow_info.addr[i].sa_remote;
        char local_ip[INET6_ADDRSTRLEN];
        char remote_ip[INET6_ADDRSTRLEN];
        int local_port;
        int remote_port;
        if (local->sa_family == AF_INET)
        {
            struct sockaddr_in *local4 = (struct sockaddr_in *)local;
            struct sockaddr_in *remote4 = (struct sockaddr_in *)remote;
            inet_ntop(AF_INET, &local4->sin_addr, local_ip, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &remote4->sin_addr, remote_ip, INET_ADDRSTRLEN);
            local_port = ntohs(local4->sin_port);
            remote_port = ntohs(remote4->sin_port);
        }
        else if (local->sa_family == AF_INET6)
        {
            struct sockaddr_in6 *local6 = (struct sockaddr_in6 *)local;
            struct sockaddr_in6 *remote6 = (struct sockaddr_in6 *)remote;
            inet_ntop(AF_INET6, &local6->sin6_addr, local_ip, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &remote6->sin6_addr, remote_ip, INET6_ADDRSTRLEN);
            local_port = ntohs(local6->sin6_port);
            remote_port = ntohs(remote6->sin6_port);
        }
        else
        {
            printf("Unknown address family: %d, skipping\n", local->sa_family);
            continue;
        }
        // Populate the dictionary with the addresses
        PyDict_SetItemString(subflow_dict, "local_addr", PyUnicode_FromString(local_ip));
        PyDict_SetItemString(subflow_dict, "local_port", PyLong_FromLong(local_port));
        PyDict_SetItemString(subflow_dict, "remote_addr", PyUnicode_FromString(remote_ip));
        PyDict_SetItemString(subflow_dict, "remote_port", PyLong_FromLong(remote_port));
        // Add the dictionary to the list
        PyList_SetItem(subflow_list, i, subflow_dict);
    }

    // Return the list of subflow dictionaries
    return subflow_list;
}

PyObject * mptcp_util_get_subflow_tcp_info(PyObject *self, PyObject *args)
{
    int sockfd;
    if (!PyArg_ParseTuple(args, "i", &sockfd))
        return NULL;
        
    struct subflow_addrs
    {
        struct mptcp_subflow_data d;
        struct tcp_info ti[2];
    } addrs;

    memset(&addrs, 0, sizeof(addrs));

    addrs.d.size_subflow_data = sizeof(struct mptcp_subflow_data);
    addrs.d.size_user = sizeof(struct tcp_info);

    socklen_t len = sizeof(addrs);
    // Get sock opt for subflow tcp info
    int ret = getsockopt(sockfd, SOL_MPTCP, MPTCP_TCPINFO, &addrs, &len);
    if (ret < 0)
    {
        int err = errno;
        char *err_msg = strerror(err);
        char *err_msg_full = malloc(strlen(err_msg) + 100);
        sprintf(err_msg_full, "Error getting subflow tcp info: [%d] %s", err, err_msg);
        PyErr_SetString(PyExc_RuntimeError, err_msg_full);
        free(err_msg_full);
        return NULL;
    }
    // Create a list to hold all subflows as individual dictionaries
    int num_subflows = (int)(addrs.d.num_subflows);
    PyObject *subflow_list = PyList_New(0);
    // Iterate through each subflow and add it to the list
    for (int i = 0; i < num_subflows; i++)
    {
        PyObject *subflow_dict = PyDict_New();
        // Add the subflow id (we make up this id, it's not a real attribute)
        PyDict_SetItemString(subflow_dict, "id", PyLong_FromLong(i));


        // Get the tcp info
        struct tcp_info *ti = &addrs.ti[i];
        // Populate the dictionary with the tcp info
        PyDict_SetItemString(subflow_dict, "tcpi_state", PyLong_FromLong(ti->tcpi_state));
        PyDict_SetItemString(subflow_dict, "tcpi_ca_state", PyLong_FromLong(ti->tcpi_ca_state));
        PyDict_SetItemString(subflow_dict, "tcpi_retransmits", PyLong_FromLong(ti->tcpi_retransmits));
        PyDict_SetItemString(subflow_dict, "tcpi_probes", PyLong_FromLong(ti->tcpi_probes));
        PyDict_SetItemString(subflow_dict, "tcpi_backoff", PyLong_FromLong(ti->tcpi_backoff));
        PyDict_SetItemString(subflow_dict, "tcpi_options", PyLong_FromLong(ti->tcpi_options));
        PyDict_SetItemString(subflow_dict, "tcpi_snd_wscale", PyLong_FromLong(ti->tcpi_snd_wscale));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_wscale", PyLong_FromLong(ti->tcpi_rcv_wscale));
        PyDict_SetItemString(subflow_dict, "tcpi_delivery_rate_app_limited", PyLong_FromLong(ti->tcpi_delivery_rate_app_limited));
        PyDict_SetItemString(subflow_dict, "tcpi_fastopen_client_fail", PyLong_FromLong(ti->tcpi_fastopen_client_fail));
        PyDict_SetItemString(subflow_dict, "tcpi_rto", PyLong_FromLong(ti->tcpi_rto));
        PyDict_SetItemString(subflow_dict, "tcpi_ato", PyLong_FromLong(ti->tcpi_ato));
        PyDict_SetItemString(subflow_dict, "tcpi_snd_mss", PyLong_FromLong(ti->tcpi_snd_mss));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_mss", PyLong_FromLong(ti->tcpi_rcv_mss));
        PyDict_SetItemString(subflow_dict, "tcpi_unacked", PyLong_FromLong(ti->tcpi_unacked));
        PyDict_SetItemString(subflow_dict, "tcpi_sacked", PyLong_FromLong(ti->tcpi_sacked));
        PyDict_SetItemString(subflow_dict, "tcpi_lost", PyLong_FromLong(ti->tcpi_lost));
        PyDict_SetItemString(subflow_dict, "tcpi_retrans", PyLong_FromLong(ti->tcpi_retrans));
        PyDict_SetItemString(subflow_dict, "tcpi_fackets", PyLong_FromLong(ti->tcpi_fackets));
        PyDict_SetItemString(subflow_dict, "tcpi_last_data_sent", PyLong_FromLong(ti->tcpi_last_data_sent));
        PyDict_SetItemString(subflow_dict, "tcpi_last_ack_sent", PyLong_FromLong(ti->tcpi_last_ack_sent));
        PyDict_SetItemString(subflow_dict, "tcpi_last_data_recv", PyLong_FromLong(ti->tcpi_last_data_recv));
        PyDict_SetItemString(subflow_dict, "tcpi_last_ack_recv", PyLong_FromLong(ti->tcpi_last_ack_recv));
        PyDict_SetItemString(subflow_dict, "tcpi_pmtu", PyLong_FromLong(ti->tcpi_pmtu));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_ssthresh", PyLong_FromLong(ti->tcpi_rcv_ssthresh));
        PyDict_SetItemString(subflow_dict, "tcpi_rtt", PyLong_FromLong(ti->tcpi_rtt));
        PyDict_SetItemString(subflow_dict, "tcpi_rttvar", PyLong_FromLong(ti->tcpi_rttvar));
        PyDict_SetItemString(subflow_dict, "tcpi_snd_ssthresh", PyLong_FromLong(ti->tcpi_snd_ssthresh));
        PyDict_SetItemString(subflow_dict, "tcpi_snd_cwnd", PyLong_FromLong(ti->tcpi_snd_cwnd));
        PyDict_SetItemString(subflow_dict, "tcpi_advmss", PyLong_FromLong(ti->tcpi_advmss));
        PyDict_SetItemString(subflow_dict, "tcpi_reordering", PyLong_FromLong(ti->tcpi_reordering));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_rtt", PyLong_FromLong(ti->tcpi_rcv_rtt));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_space", PyLong_FromLong(ti->tcpi_rcv_space));
        PyDict_SetItemString(subflow_dict, "tcpi_total_retrans", PyLong_FromLong(ti->tcpi_total_retrans));
        PyDict_SetItemString(subflow_dict, "tcpi_pacing_rate", PyLong_FromLong(ti->tcpi_pacing_rate));
        PyDict_SetItemString(subflow_dict, "tcpi_max_pacing_rate", PyLong_FromLong(ti->tcpi_max_pacing_rate));
        PyDict_SetItemString(subflow_dict, "tcpi_bytes_acked", PyLong_FromLong(ti->tcpi_bytes_acked));
        PyDict_SetItemString(subflow_dict, "tcpi_bytes_received", PyLong_FromLong(ti->tcpi_bytes_received));
        PyDict_SetItemString(subflow_dict, "tcpi_segs_out", PyLong_FromLong(ti->tcpi_segs_out));
        PyDict_SetItemString(subflow_dict, "tcpi_segs_in", PyLong_FromLong(ti->tcpi_segs_in));
        PyDict_SetItemString(subflow_dict, "tcpi_notsent_bytes", PyLong_FromLong(ti->tcpi_notsent_bytes));
        PyDict_SetItemString(subflow_dict, "tcpi_min_rtt", PyLong_FromLong(ti->tcpi_min_rtt));
        PyDict_SetItemString(subflow_dict, "tcpi_data_segs_in", PyLong_FromLong(ti->tcpi_data_segs_in));
        PyDict_SetItemString(subflow_dict, "tcpi_data_segs_out", PyLong_FromLong(ti->tcpi_data_segs_out));
        PyDict_SetItemString(subflow_dict, "tcpi_delivery_rate", PyLong_FromLong(ti->tcpi_delivery_rate));
        PyDict_SetItemString(subflow_dict, "tcpi_busy_time", PyLong_FromLong(ti->tcpi_busy_time));
        PyDict_SetItemString(subflow_dict, "tcpi_rwnd_limited", PyLong_FromLong(ti->tcpi_rwnd_limited));
        PyDict_SetItemString(subflow_dict, "tcpi_sndbuf_limited", PyLong_FromLong(ti->tcpi_sndbuf_limited));
        PyDict_SetItemString(subflow_dict, "tcpi_delivered", PyLong_FromLong(ti->tcpi_delivered));
        PyDict_SetItemString(subflow_dict, "tcpi_delivered_ce", PyLong_FromLong(ti->tcpi_delivered_ce));
        PyDict_SetItemString(subflow_dict, "tcpi_bytes_sent", PyLong_FromLong(ti->tcpi_bytes_sent));
        PyDict_SetItemString(subflow_dict, "tcpi_bytes_retrans", PyLong_FromLong(ti->tcpi_bytes_retrans));
        PyDict_SetItemString(subflow_dict, "tcpi_dsack_dups", PyLong_FromLong(ti->tcpi_dsack_dups));
        PyDict_SetItemString(subflow_dict, "tcpi_reord_seen", PyLong_FromLong(ti->tcpi_reord_seen));
        PyDict_SetItemString(subflow_dict, "tcpi_rcv_ooopack", PyLong_FromLong(ti->tcpi_rcv_ooopack));


        // Add the subflow dictionary to the list
        PyList_Append(subflow_list, subflow_dict);
    }
    
    return subflow_list;
}

// Module method table
static PyMethodDef MptcpUtilMethods[] = {
    {"get_mptcp_info", mptcp_util_get_mptcp_info, METH_VARARGS, "Get MPTCP information for a given socket file descriptor"},
    {"get_subflow_info", mptcp_util_get_subflow_info, METH_VARARGS, "Get subflow information for a given socket file descriptor"},
    {"get_subflow_tcp_info", mptcp_util_get_subflow_tcp_info, METH_VARARGS, "Get subflow TCP information for a given socket file descriptor"},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

// Module definition
static struct PyModuleDef mptcp_util_module = {
    PyModuleDef_HEAD_INIT,
    "mptcp_util", /* name of module */
    NULL,         /* module documentation, may be NULL */
    -1,           /* size of per-interpreter state of the module,
                     or -1 if the module keeps state in global variables. */
    MptcpUtilMethods};

// Module initialization
PyMODINIT_FUNC PyInit_mptcp_util(void)
{
    return PyModule_Create(&mptcp_util_module);
}