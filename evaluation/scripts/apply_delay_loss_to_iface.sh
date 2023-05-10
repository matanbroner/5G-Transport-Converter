#!/bin/bash

# Check if all three arguments are provided
if [ $# -ne 3 ]; then
    echo "Usage: $0 <interface> <delay> <loss>"
    exit 1
fi

# Assign command line arguments to variables
interface=$1
delay=$2
loss=$3

# Remove existing ingress delay and loss configurations
tc qdisc del dev $interface ingress 2>/dev/null

# Remove existing egress delay and loss configurations
tc qdisc del dev $interface root netem 2>/dev/null

# Create IFB device
modprobe ifb numifbs=1
ip link set dev ifb0 up

# Add delay and loss to ingress
tc qdisc add dev $interface ingress
tc filter add dev $interface parent ffff: protocol ip prio 50 u32 match ip dst 0.0.0.0/0 flowid :1 action mirred egress redirect dev ifb0
tc qdisc add dev ifb0 handle 1: root netem delay $delay loss $loss

# Add delay and loss to egress
tc qdisc add dev $interface root netem delay $delay loss $loss

echo "Delay of $delay ms and loss of $loss added to both ingress and egress of $interface via IFB mirroring."
