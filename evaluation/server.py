import socket
import os
import sys
import threading
import logging
from collections import namedtuple

logger = logging.getLogger("mptcp_server")

MAGIC_NUMBER = 1423

CLIENT_TYPES = {
    0: "UPLINK",
    1: "DOWNLINK",
    2: "ECHO"
}

def generate_random_data_buffer(size):
    return os.urandom(size)

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
        magic_number = int.from_bytes(magic_number, "big")
        if magic_number != MAGIC_NUMBER:
            logger.error("Invalid magic number: %d" % magic_number)
            return None
        # Read client type (1 byte)
        client_type = sock.recv(1)
        client_type = int.from_bytes(client_type, "big")
        if client_type not in CLIENT_TYPES:
            logger.error("Invalid client type: %d" % client_type)
            return None
        # Read buffer size (4 bytes)
        buffer_size = sock.recv(4)
        buffer_size = int.from_bytes(buffer_size, "big")
        if buffer_size < 0:
            logger.error("Invalid buffer size: %d" % buffer_size)
            return None
        return ClientConnection(sock, client_type, buffer_size)


    def handle_client(self, conn: ClientConnection):
        # If client is an echo client, just echo the data back as we receive it
        if conn.type == 2:
            while True:
                data = conn.socket.recv(conn.buffer_size)
                if not data:
                    break
                conn.socket.sendall(data)
        # If client is an uplink client, read data from the socket and never send anything back
        elif conn.type == 0:
            while True:
                data = conn.socket.recv(conn.buffer_size)
                if not data:
                    break
        # If client is a downlink client, send random data to the client
        elif conn.type == 1:
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