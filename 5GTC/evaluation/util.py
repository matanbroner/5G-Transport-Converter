import os

MAGIC_NUMBER = 0x7F
DEFAULT_BUFFER_SIZE = 1024

CLIENT_TYPE_UPLINK = 0
CLIENT_TYPE_DOWNLINK = 1
CLIENT_TYPE_ECHO = 2

COLORS = {
    "RED": "\033[91m",
    "GREEN": "\033[92m",
    "YELLOW": "\033[93m",
    "BLUE": "\033[94m",
    "PURPLE": "\033[95m",
    "CYAN": "\033[96m",
    "WHITE": "\033[97m",
    "NONE": "\033[0m",
}

def generate_random_data_buffer(size):
    data = [b'\x01'] * size
    return bytes(data)
