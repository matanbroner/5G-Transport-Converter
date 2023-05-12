import socket
import threading
import subprocess
from pyroute2 import NDB
from mptcp_util import *

IPV4 = 2

# TODO: configure this through command line arguments
DEFAULT_NETWORK_INTERFACE = "enp0s8"
DEFAULT_NETWORK_IP = "192.168.2.221"

class SysctlContext:
    """
    Allows execution of code with a temporary sysctl variable value.
    Example:
    with SysctlContext("net.mptcp.mptcp_enabled", 0):
        # Code that runs with mptcp disabled
    """
    def __init__(self, key, value):
        self.key = key
        self.value = value

    def __enter__(self):
        # Save the current value of the sysctl variable
        self.old_value = subprocess.check_output(['sysctl', '-n', self.key]).decode().strip()

        # Set the new value of the sysctl variable
        subprocess.check_call(['sysctl', '-w', f"{self.key}={self.value}"])

        #  Read back the value to make sure it was set correctly
        new_value = subprocess.check_output(['sysctl', '-n', self.key]).decode().strip()

        # Print the old and new values
        print(f"sysctl {self.key} was {self.old_value}, now {new_value}")

    def __exit__(self, exc_type, exc_val, exc_tb):
        # Restore the old value of the sysctl variable
        subprocess.check_call(['sysctl', '-w', f"{self.key}={self.old_value}"])

class TestMPTCPServer:
    """
    This class is used to create a MPTCP server for testing purposes.
    It starts a server in a background thread and listens for incoming connections.
    After receiving a connection, it echoes the data back to the client.
    """

    def __init__(self, ip):
        self.ip = ip
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_MPTCP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.ip, 0))
        self.port = None

    def __del__(self):
        self.sock.close()
        if self.thread:
            self.thread.join()
    
    def listen(self):
        # Start the server in a background thread and store the thread object
        self._running = True
        self.thread = threading.Thread(target=self._listen)
        self.thread.start()
        # Wait for the server to start listening, blocking call
        while self.port == None:
            pass

    def close(self):
        self._running = False
        self.sock.close()
        if self.thread:
            self.thread.join()

    def _listen(self):
        self.sock.listen(10)
        self.port = self.sock.getsockname()[1]
        while self._running:
            conn, addr = self.sock.accept()
            # Mirror the data back to the client
            data = conn.recv(1024)
            if not data:
                break
            conn.sendall(data)
            conn.close()


class TestMPTCPClient:
    """
    This class is used to create a MPTCP client for testing purposes.
    It connects to a given server and sends data to it.
    """
    def __init__(self, server_ip, server_port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_MPTCP)
        self.sock.connect((server_ip, server_port))

    def __del__(self):
        self.close()

    def close(self):
        self.sock.close()

    def send(self, data):
        self.sock.sendall(data)

    def is_connected(self):
        return self.sock.fileno() != -1

def interface_ips(ifname):
    """
    Return a list of IP addresses for the given interface name.
    """
    ips = []
    with NDB() as ndb:
        # Get the interface object by name
        iface = ndb.interfaces.get(ifname)

        if not iface:
            raise Exception("Interface not found: {}".format(ifname))

        # Get all IP addresses (both virtual and not) for the interface
        for ipaddr in iface.ipaddr:
            #  If IP address is IPV4, add it to the list
            if ipaddr['family'] == IPV4:
                ips.append(ipaddr['address'])

    return ips

def mptcp_info_get_attr(sock, attr):
    """
    Get the value of a MPTCP socket attribute.
    """
    # Get the MPTCP connection token
    attr = 'mptcpi_' + attr
    mptcp_info = get_mptcp_info(sock.fileno())
    return mptcp_info[attr]