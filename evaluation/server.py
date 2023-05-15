import socket
import sys
import threading
import logging
from collections import namedtuple
from util import (
    MAGIC_NUMBER,
    DEFAULT_BUFFER_SIZE,
    CLIENT_TYPE_UPLINK,
    CLIENT_TYPE_DOWNLINK,
    CLIENT_TYPE_ECHO,
    generate_random_data_buffer
)

logger = logging.getLogger("mptcp_server")

ClientConnection = namedtuple("ClientConnection", ["socket", "type", "buffer_size"])

class MPTCPServer:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_MPTCP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.host, self.port))
        self.sock.listen(200)
        self.threads = []

    def __del__(self):
        self.cleanup()

    def run(self):
        logger.info("Listening on %s:%d" % (self.host, self.port))
        while True:
            client, addr = self.sock.accept()
            logger.info("New connection from %s:%d" % addr)
            # Create connection object
            conn = self.read_client_header(client)
            if conn is None:
                client.close()
                continue
            # Handle connection
            t = threading.Thread(target=self.handle_client, args=(conn,)).start()
            self.threads.append(t)
    
    def cleanup(self):
        for t in self.threads:
            t.join()
        self.sock.close()   

    def read_client_header(self, sock):
        # Read magic number
        magic_number = sock.recv(2)
        magic_number = int.from_bytes(magic_number, "little")
        if magic_number != MAGIC_NUMBER:
            logger.error("Invalid magic number: %d" % magic_number)
            logger.error("Expected: %d" % MAGIC_NUMBER)
            return None
        # Read client type (1 byte)
        client_type = sock.recv(1)
        client_type = int.from_bytes(client_type, "little")
        if client_type not in [CLIENT_TYPE_UPLINK, CLIENT_TYPE_DOWNLINK, CLIENT_TYPE_ECHO]]:
            logger.error("Invalid client type: %d" % client_type)
            return None
        # Read buffer size (4 bytes)
        buffer_size = sock.recv(4)
        buffer_size = int.from_bytes(buffer_size, "little")
        if buffer_size < 0:
            logger.error("Invalid buffer size: %d. Using default buffer size." % buffer_size)
            buffer_size = DEFAULT_BUFFER_SIZE
        return ClientConnection(sock, client_type, buffer_size)


    def handle_client(self, conn: ClientConnection):
        # Send magic number first if client is an uplink client or a downlink client
        if conn.type in [CLIENT_TYPE_UPLINK, CLIENT_TYPE_DOWNLINK]:
            conn.socket.sendall(MAGIC_NUMBER.to_bytes(2, "little"))
        
        # If client is an echo client, just echo the data back as we receive it
        if conn.type == CLIENT_TYPE_ECHO:
            #  Be the first to send data
            data = generate_random_data_buffer(conn.buffer_size)
            conn.socket.sendall(data)
            while True:
                data = conn.socket.recv(conn.buffer_size)
                if not data:
                    break
                conn.socket.sendall(data)
        # If client is an uplink client, read data from the socket and never send anything back
        elif conn.type == CLIENT_TYPE_UPLINK:
            while True:
                data = conn.socket.recv(conn.buffer_size)
                if not data:
                    break
        # If client is a downlink client, send random data to the client
        elif conn.type == CLIENT_TYPE_DOWNLINK:
            while True:
                data = generate_random_data_buffer(conn.buffer_size)
                conn.socket.sendall(data)
        
if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    if len(sys.argv) != 3:
        print("Usage: python3 server.py <host> <port>")
        sys.exit(1)
    host, port = sys.argv[1], int(sys.argv[2])
    server = MPTCPServer(host, port)
    server.run()