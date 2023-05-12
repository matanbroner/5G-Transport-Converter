# Tests for modified Pyroute2 library

import unittest
import pyroute2.netlink.generic.mptcp as mptcp
from test.util import (
    SysctlContext,
    TestMPTCPServer, 
    TestMPTCPClient,
    interface_ips, 
    mptcp_info_get_attr,
    DEFAULT_NETWORK_INTERFACE, 
    DEFAULT_NETWORK_IP
)

class TestMPTCP(unittest.TestCase):
    def setUp(self):
        self.mptcp = mptcp.MPTCP()

    def tearDown(self):
        self.mptcp.close()

    def test_create(self):
        # Sanity check
        self.assertIsInstance(self.mptcp, mptcp.MPTCP)

        # Check that the default network interface has more than one IP address
        self.assertGreater(len(interface_ips(DEFAULT_NETWORK_INTERFACE)), 1)
        
        # Create server
        server = TestMPTCPServer(DEFAULT_NETWORK_IP)
        server.listen()

        # Create the connection
        client = TestMPTCPClient(DEFAULT_NETWORK_IP, server.port)
        self.assertTrue(client.is_connected())

        # Create the new subflow
        token = mptcp_info_get_attr(client.sock, 'token')

        # Choose an IP that is not the IP used by the client socket already
        ips = interface_ips(DEFAULT_NETWORK_INTERFACE)
        ips.remove(DEFAULT_NETWORK_IP)
        ip = ips[0]
        with SysctlContext('net.mptcp.pm_type', 1):
            res = self.mptcp.create(
                local_ip=ip,
                local_port=0,
                local_id=1,
                remote_ip=server.ip,
                remote_port=server.port,
                token=token
            )
            print(res)


        # Kill the client and server
        client.close()
        server.close()

