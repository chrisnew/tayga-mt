#!/bin/bash
# 
# File:   tayga.bash
# Author: cr
#
# Created on Feb 15, 2019, 7:55:44 PM
#

cd "$(dirname "$0")"

set -ex

./tayga -d --nodetach --config tayga.conf & 
PID=$!

trap "kill $PID" EXIT

sleep 1

IFACE=nat64

ip link show dev $IFACE

ip link set $IFACE up
ip route add 2001:db8:1:ffff::/96 dev $IFACE
ip route add 192.168.178.192/27 dev $IFACE
ip addr add 192.168.178.193/32 dev $IFACE
ip addr add 2001:db8:1::2/128 dev $IFACE

ip link show dev $IFACE

sysctl -w net.ipv4.conf.enp3s0.proxy_arp=1
sysctl -w net.ipv4.conf.nat64.proxy_arp=1
sysctl -w net.ipv4.conf.enp3s0.forwarding=1
sysctl -w net.ipv4.conf.nat64.forwarding=1
sysctl -w net.ipv6.conf.enp3s0.forwarding=1
sysctl -w net.ipv6.conf.nat64.forwarding=1

gdb -p $PID

wait