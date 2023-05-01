# This is a MPTCP Transport Converter (TC) server written in Python.
# It uses the Convert Protocol (RFC 8083) to reroute TCP connections

# Packets are queued in a buffer from both directions of a connection
# and then sent to the other side of the connection with the addition of
# a Convert header.

# The server accepts a configuration object, sourced from a YAML file (config.yaml)


import socket
import sys
import select
import logging
import json
import time
import yaml
import os

from convert import *
from mptcp_util import *
from tcp_info import *


DEFAULT_BUFFER_SIZE = 4096

# Create logger
logging.basicConfig()
logger = logging.getLogger("[main.py]")

# Set logging level
logger.setLevel(logging.DEBUG)

class TCServer:
    def __init__(self, config):
        self.config = config

        # If config includes a log file, log to that file
        if "log" in config:
            if "level" in config["log"] and config["log"]["level"] in ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]:
                logger.setLevel(config["log"]["level"])
            if "output_to_console" in config["log"] and config["log"]["output_to_console"]:
                ch = logging.StreamHandler()
                logger.addHandler(ch)
            elif "path" in config["log"] and "filename" in config["log"]["path"]:
                # Create the complete path as needed
                if not os.path.exists(config["log"]["path"]):
                    os.makedirs(config["log"]["path"])
                fh = logging.FileHandler(config["log"]["path"] + "/" + config["log"]["filename"])
                fh.setLevel(logging.DEBUG)
                logger.addHandler(fh)
            elif "filename" in config["log"]:
                fh = logging.FileHandler(config["log"]["filename"])
                fh.setLevel(logging.DEBUG)
                logger.addHandler(fh)

        # Set proxy configuration
        self.read_buffer_size = DEFAULT_BUFFER_SIZE
        if "proxy" in config:
            if "read_buffer_size" in config["proxy"]:
                self.read_buffer_size = config["proxy"]["read_buffer_size"]

        # IP address to listen on
        self.ip = config["network"]["ip"]
        # Port to listen on
        self.port = config["network"]["port"]
        # Client socket for MPTCP connection
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_MPTCP)
        # Set socket reuse
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind((self.ip, self.port))
        self.sock.listen(200)

        # Maintain socket states
        self.forward_map = {}
        self.perf_loggers = {}

        # Input list
        self.inputs = [self.sock]
        self.track_client_sockets = {}

        # Start the server
        self.run_nonblocking()

    def __del__(self):
        self.cleanup()

    def run_nonblocking(self):
        """
        Run the server in a non-blocking manner
        Each operation is handled in a separate thread
        """
        logger.info("Server listening on {}".format(self.sock.getsockname()))
        while True:
            # Wait for input
            readable, _, _ = select.select(self.inputs, [], [])
            for s in readable:
                if s is self.sock:
                    # Accept a connection
                    client_sock, addr = self.sock.accept()

                    logger.info("Accepted connection from {} with fd={}".format(addr, client_sock.fileno()))
                    print("\t--> ", client_sock)

                    self.handle_connection(client_sock)
                else:
                    # Read from the socket
                    self.read_and_forward(s)
            

    def handle_connection(self, client_sock):
        """
        Handle a new connection from a client
        Parses the Convert header and handles the TLVs
        """
        # Read the Convert header from the client (TCP Fast Open)
        convert = read_convert_header(client_sock)
        if convert is None:
            logger.error("Error reading Convert header from client")
            return
        
        # Go through the TLVs and handle them
        for tlv in convert.tlvs:
            if CONVERT_TLVS[tlv.type] == "connect":
                self.handle_tlv_connect(tlv, client_sock)
            else:
                logger.info("Handled TLV: {}".format(CONVERT_TLVS[tlv.type]))

    def handle_tlv_connect(self, tlv, client_sock):
        """
        Handle the Connect TLV

        This TLV is sent by the client to the server to indicate that
        it wants to connect to a remote server. The server then
        connects to the remote server and sends an empty Convert
        header to the client to mimic the destination server sending
        it.
        """
        # Create a new socket to the server
        ipv4 = ipv6_to_ipv4(tlv.remote_addr)
        server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
        try:
            server_sock.connect((ipv4, tlv.remote_port))
        except Exception as e:
            logger.error("Error connecting to server: {}".format(e))
            # Close connection to client to indicate error
            client_sock.close()
            return


        # Send empty Convert header to client (to mimic destination server sending it)
        client_sock.send(Convert().build())

        # Add the server and client sockets to the input list
        self.inputs.append(server_sock)
        self.inputs.append(client_sock)
        self.track_client_sockets[client_sock.fileno()] = True
        self.track_client_sockets[server_sock.fileno()] = False
        
        # Add the client socket to the forward map
        self.forward_map[client_sock.fileno()] = server_sock
        # Add the server socket to the forward map
        self.forward_map[server_sock.fileno()] = client_sock

        self.perf_loggers[client_sock.fileno()] = PerformanceLogger(client_sock)
        self.perf_loggers[client_sock.fileno()].run(
            interval_ms=self.config["performance"]["measurement_interval_ms"],
            features=self.config["performance"]["tcp_subflow_info_features"],
        )

    def read_and_forward(self, sock):
        """
        Read from the socket and forward the data to its corresponding
        socket. If the socket is closed, close the other socket and
        remove them from the input list and forward map. 
        """
        cfd, sfd = sock, self.forward_map[sock.fileno()]
        if sfd.fileno() == -1 or cfd.fileno() == -1:
            # Close both sockets
            logger.info("Socket pair is closed, closing sockets")
            self.cleanup_socket_pair(cfd, sfd)
        try:
            data = sock.recv(self.read_buffer_size)
        except Exception as e:
            logger.error(e)
            self.cleanup_socket_pair(cfd, sfd)
            return
        if data:
            # Forward data to the other socket
            self.forward_map[sock.fileno()].send(data)
        else:
            self.cleanup_socket_pair(cfd, sfd)

    def cleanup_socket_pair(self, cfd, sfd):
        """
        Close both sockets and remove them from the input list and
        forward map. 
        """
        logger.info("Closing socket pair (%d, %d)" % (cfd.fileno(), sfd.fileno()))
        # Remove the socket from the input list
        self.inputs.remove(cfd)
        # Remove the socket from the forward map
        del self.forward_map[cfd.fileno()]

        # Close and remove the other socket
        del self.forward_map[sfd.fileno()]
        self.inputs.remove(sfd)

        # Print RTT values
        perf_logger = self.perf_loggers.get(cfd.fileno(), None) or self.perf_loggers.get(sfd.fileno(), None)
        if perf_logger:
            cfd_ip = cfd.getpeername()[0]
            sfd_ip = sfd.getpeername()[0]
            filename = "performance_{0}_{1}.json".format(cfd_ip, sfd_ip)
            perf_logger.stop(filename=filename)

        # Close the sockets
        cfd.close()
        sfd.close()
            
    def cleanup(self):
        """
        Tear down the server
        """
        for s in self.inputs:
            s.close()


class PerformanceLogger:
    """
    This class is used to log performance metrics for an MPTCP connection
    It exposes a run a background thread that periodically logs the metrics
    and can be stopped by calling stop() which writes the metrics to a file 
    specified by the user.
    """
    def __init__(self, sock):
        self.sock = sock
        self.data = {}

    def run(self, interval_ms=1, features=None):
        """
        Run the logger in a background thread
        """
        logger.info("PerformanceLogger [fid: {}] Started".format(self.sock.fileno()))
        logger.info("PerformanceLogger [fid: {}] Logging interval: {} ms".format(self.sock.fileno(), interval_ms))
        logger.info("PerformanceLogger [fid: {}] Logging features: {}".format(self.sock.fileno(), features))

        self.interval_ms = interval_ms / 1000
        self.thread = threading.Thread(target=self._run_logger, args=(self.interval_ms, features,))
        self._thread_run = True
        self.thread.start()

    def stop(self, filename=None):
        if filename is None:
            filename = "performance_log.json"
        # Stop the logger thread
        self._thread_run = False
        self.thread.join()
        # Write the data to a file
        with open(filename, "w") as f:
            #  Pretty print the data in JSON 
            f.write(json.dumps(self.data, indent=4))
        logger.info("PerformanceLogger [fid: {}] Stopped".format(self.sock.fileno()))
        logger.info("PerformanceLogger [fid: {}] Data written to {}".format(self.sock.fileno(), filename))


    def _run_logger(self, interval_ms, features=None):
        iterations = 0
        if features is None:
            features = ["tcpi_rtt"]
        while self._thread_run:
            iterations += 1
            logger.info("PerformanceLogger [fid: {}] Iteration {}".format(self.sock.fileno(), iterations))
            # Get the subflow TCP info
            subflow_tcp_info = get_subflow_tcp_info(self.sock.fileno())
            logger.info("PerformanceLogger [fid: {}] Subflows: {}".format(self.sock.fileno(), len(subflow_tcp_info)))
            for subflow in subflow_tcp_info:
                if isinstance(subflow, dict):
                    # If the subflow is not in the data dict, add it
                    if subflow["id"] not in self.data:
                        self.data[subflow["id"]] = {}
                    # Add the features to the data dict
                    for feature in features:
                        if feature in subflow:
                            if feature not in self.data[subflow["id"]]:
                                self.data[subflow["id"]][feature] = []
                            self.data[subflow["id"]][feature].append(subflow[feature])
            # Sleep for the interval assigned
            time.sleep(interval_ms)


if __name__ == "__main__":
    # Load the config file
    with open("config.yaml", "r") as f:
        config = yaml.load(f, Loader=yaml.FullLoader)
    server = TCServer(config)


        
