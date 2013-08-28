#!/bin/sh
IPTABLES='/sbin/iptables'

# Set interface values
EXTIF='hso0'
INTIF='usb0'

# enable ip forwarding in the kernel
/bin/echo 1 > /proc/sys/net/ipv4/ip_forward

# flush rules and delete chains
$IPTABLES -F
$IPTABLES -X

#Enable masquerading to allow LAN internet access
$IPTABLES -t nat -A POSTROUTING -o $EXTIF -j MASQUERADE

#Forward LAN traffic from LAN $INTIF to Internet $EXTIF
$IPTABLES -A FORWARD -i $INTIF -o $EXTIF -m state --state NEW,ESTABLISHED -j ACCEPT

#Allowing access to the SSH server"
#$IPTABLES -A INPUT --protocol tcp --dport 22 -j ACCEPT

#Allowing access to the HTTP server"
#$IPTABLES -A INPUT --protocol tcp --dport 80 -j ACCEPT

# block out all other Internet access on $EXTIF
$IPTABLES -A INPUT -i $EXTIF -m state --state NEW,INVALID -j DROP
$IPTABLES -A FORWARD -i $EXTIF -m state --state NEW,INVALID -j DROP

