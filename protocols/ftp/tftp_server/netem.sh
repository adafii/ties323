#!/bin/sh

# Packet delay (delay jitter), loss, duplication and reordering
doas tc qdisc add dev lo root netem delay 100ms 50ms loss 20% duplicate 20% reorder 20%

# To clear: doas tc qdisc del dev lo root netem
