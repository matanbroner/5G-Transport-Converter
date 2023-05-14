# A standalone module to run the PerformanceLogger 
# outside of a Transport Converter server
# This allows us to measure TCP info for any given file descriptor

import sys
import argparse
from pkg.performance_logger import PerformanceLogger, WebUI

if __name__ == "__main__":
    if len(sys.argv) < 2 or "".join(sys.argv).find("--help") != -1:
        print("Usage: python3 logger.py <socket fd> [--webui-host <host>] [--webui-port <port>] [--interval <interval>] [--features <features>] [--log <log>]")
        print("\t<socket fd>: Socket file descriptor to log performance metrics for")
        print("\t--webui-host <host>: Host to run the webui on")
        print("\t--webui-port <port>: Port to run the webui on")
        print("\t--interval <interval>: Interval in ms to log the performance metrics")
        print("\t--features <features>: Features to log")
        print("\t--log <log>: Log level")
        print("Example: python3 logger.py 3 --webui-host localhost --webui-port 5000 --interval 1000 --features all --log INFO")

        sys.exit(1)
    # Read the command line arguments
    parser = argparse.ArgumentParser(description="Performance Logger")
    parser.add_argument("sockfd", type=int, help="Socket file descriptor")
    parser.add_argument("--webui-host", type=str, default="localhost", help="Host to run the webui on")
    parser.add_argument("--webui-port", type=int, default=5000, help="Port to run the webui on")
    parser.add_argument("--interval", type=int, default=1000, help="Interval in ms to log the performance metrics")
    parser.add_argument("--features", type=str, default="all", help="Features to log")
    parser.add_argument("--log", type=str, default="INFO", help="Log level")
    args = parser.parse_args()

    # Initialize the WebUI module
    webui = WebUI(args.host, args.port, log=args.log)

    # Feature split by comma
    features = args.features.split(",")

    # Initialize the PerformanceLogger
    logger = PerformanceLogger(args.sockfd, log=args.log)

    # Run the logger
    logger.run(interval_ms=args.interval, features=features)