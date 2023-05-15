#!/bin/bash

# Check if the interface argument is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <interface>"
    exit 1
fi

# Assign command line argument to variable
interface=$1

# Remove ingress delay and loss configurations
tc qdisc del dev "$interface" ingress 2>/dev/null

# Remove egress delay and loss configurations
tc qdisc del dev "$interface" root netem 2>/dev/null

# Remove IFB device
ip link set dev ifb0 down
modprobe -r ifb

echo "Reverse script executed. Delay and loss configurations removed from both ingress and egress of $interface. IFB device removed."