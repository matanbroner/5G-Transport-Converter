import socket
import logging
import threading
import signal
import time
import sys
import argparse

from util import (
    generate_random_data_buffer, 
    MAGIC_NUMBER, 
    DEFAULT_BUFFER_SIZE,
    CLIENT_TYPE_UPLINK,
    CLIENT_TYPE_DOWNLINK,
    CLIENT_TYPE_ECHO,
    COLORS  
)

logger = logging.getLogger("mptcp_client")

def log(msg, level="INFO"):
    color = COLORS["NONE"]
    if level == "INFO":
        color = COLORS["GREEN"]
    elif level == "WARNING":
        color = COLORS["YELLOW"]
    elif level == "ERROR":
        color = COLORS["RED"]
    logger.log(level, "%s%s%s" % (color, msg, COLORS["NONE"]))

class MPTCPClient:
    def __init__(self, bind, host, port, client_type=CLIENT_TYPE_ECHO, buffer_size=DEFAULT_BUFFER_SIZE):
        self.host = host
        self.port = port
        self.client_type = client_type
        self.buffer_size = buffer_size
        self.bind = bind
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_MPTCP)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(self.bind)
        self.sock.connect((self.host, self.port))
        self._read_data = b""
        self._total_bytes_sent = 0
        self._total_bytes_received = 0
        self.loop = True
        self.authenticate()

    def __del__(self):
        self.sock.close()
        if self.thread:
            self.thread.join()

    def authenticate(self):
        log("Authenticating with server...", "INFO")
        # Send magic number
        self.sock.send(MAGIC_NUMBER.to_bytes(2, "little"))
        # Send cllog("Total bytes sent: %d" % self._total_bytes_sent)
        log("Total bytes received: %d" % self._total_bytes_received)
        self.sock.send(self.client_type.to_bytes(1, "little"))
        # Send buffer size
        self.sock.send(self.buffer_size.to_bytes(4, "little"))

    def run(self):
        # Start a background thread to report metrics
        self.thread = threading.Thread(target=self.report_metrics)
        self.thread.start()
        # Set a SIGINT handler to stop the client
        signal.signal(signal.SIGINT, self.stop)
        while self.loop:
            if self.client_type in [CLIENT_TYPE_DOWNLINK, CLIENT_TYPE_ECHO]:
                self.handle_downlink()
            if self.client_type in [CLIENT_TYPE_UPLINK, CLIENT_TYPE_ECHO]:
                self.handle_uplink()

    def stop(self, signum, frame):
        self.loop = False
        log("Stopping client...", "WARNING")

    def handle_downlink(self):
        """
        Handle downlink traffic. Store received data in self._read_data.
        """
        data = self.sock.recv(self.buffer_size)
        self._read_data = data
        self._total_bytes_received += len(data)

    def handle_uplink(self):
        """
        Handle uplink traffic. Send random data from buffer.
        """
        if not self._read_data:
            data = generate_random_data_buffer(self.buffer_size)
        else:
            data = self._read_data
        self.sock.send(data)
        self._total_bytes_sent += len(data)

    def report_metrics(self):
        """
        Report metrics to stdout.
        """
        while self.loop:
            log("Total bytes sent: %d" % self._total_bytes_sent, "INFO")
            log("Total bytes received: %d" % self._total_bytes_received, "INFO")
            time.sleep(1)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 client.py <host> <port> [--bind <bind>] [--client-type <client-type>] [--buffer-size <buffer-size>]")
        print("\t<host>: Host to connect to")
        print("\t<port>: Port to connect to")
        print("\t--bind <ip>: Bind to this address")
        print("\t--client-type <client-type>: Client type [0: uplink, 1: downlink, 2: echo]")
        print("\t--buffer-size <buffer-size>: Buffer size")
        sys.exit(1)
    parser = argparse.ArgumentParser(description="MPTCP Client")
    parser.add_argument("host", type=str, help="Host to connect to")
    parser.add_argument("port", type=int, help="Port to connect to")
    parser.add_argument("--bind", type=str, help="Bind to this address")
    parser.add_argument("--client-type", type=int, help="Client type")
    parser.add_argument("--buffer-size", type=int, help="Buffer size")
    args = parser.parse_args()

    if not args.bind:
        log("No bind address specified. Using 0.0.0.0", "WARNING")
        args.bind = "0.0.0.0"
    if not args.client_type or args.client_type not in [CLIENT_TYPE_UPLINK, CLIENT_TYPE_DOWNLINK, CLIENT_TYPE_ECHO]:
        log("No client type specified. Using echo client", "WARNING")
        args.client_type = CLIENT_TYPE_ECHO
    if not args.buffer_size:
        log("No buffer size specified. Using default buffer size", "WARNING")
        args.buffer_size = DEFAULT_BUFFER_SIZE
    (args.bind, 0)
    client = MPTCPClient((args.bind, 0), args.host, args.port, args.client_type, args.buffer_size)
    client.run()


    
    