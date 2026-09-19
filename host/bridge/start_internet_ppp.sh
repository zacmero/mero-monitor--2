#!/usr/bin/env bash
# ==============================================================================
# start_internet_ppp.sh - Host Internet & IP Bridge for Windows CE 5.0 (ActiveSync)
# Target Device: Foston FS-460BT (MediaTek MT3351)
# Project: mero-monitor-#2
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${CYAN}======================================================${NC}"
echo -e "${CYAN}   MERO MONITOR #2: USB INTERNET BRIDGE (ActiveSync)  ${NC}"
echo -e "${CYAN}======================================================${NC}"

# 1. Require root privileges
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}[!] This script must be run as root (or with sudo).${NC}"
    echo -e "    Usage: sudo $0 [tty_device]"
    exit 1
fi

# 2. Check dependencies
for cmd in pppd chat iptables sysctl; do
    if ! command -v "$cmd" &>/dev/null; then
        echo -e "${RED}[!] Missing required tool: $cmd${NC}"
        exit 1
    fi
done

# 3. Detect serial device
SERIAL_DEV="${1:-}"
if [[ -z "$SERIAL_DEV" ]]; then
    if [[ -e /dev/ttyUSB0 ]]; then
        SERIAL_DEV="/dev/ttyUSB0"
    elif [[ -e /dev/ttyUSB1 ]]; then
        SERIAL_DEV="/dev/ttyUSB1"
    else
        echo -e "${YELLOW}[*] Waiting for ActiveSync serial device (/dev/ttyUSB0)...${NC}"
        echo -e "    Ensure the GPS device is set to 'Sync' (ActiveSync) mode in its USB settings"
        echo -e "    and connected via USB."
        while true; do
            for dev in /dev/ttyUSB*; do
                if [[ -e "$dev" ]]; then
                    SERIAL_DEV="$dev"
                    break 2
                fi
            done
            sleep 1
        done
    fi
fi

echo -e "${GREEN}[+] ActiveSync serial device detected: ${SERIAL_DEV}${NC}"

# 4. Detect host WAN interface with default internet route
WAN_IFACE="$(ip route get 1.1.1.1 2>/dev/null | grep -Po '(?<=dev )\S+' || true)"
if [[ -z "$WAN_IFACE" ]]; then
    echo -e "${YELLOW}[!] Warning: Could not detect WAN interface. Internet routing may fail.${NC}"
    WAN_IFACE="eth0"
else
    echo -e "${GREEN}[+] Upstream WAN interface: ${WAN_IFACE}${NC}"
fi

HOST_IP="192.168.55.101"
CLIENT_IP="192.168.55.100"
DNS_SERVER="1.1.1.1"

# 5. Cleanup function on exit
cleanup() {
    echo -e "\n${YELLOW}[*] Shutting down bridge and cleaning iptables NAT rules...${NC}"
    iptables -t nat -D POSTROUTING -s 192.168.55.0/24 -o "$WAN_IFACE" -j MASQUERADE 2>/dev/null || true
    iptables -D FORWARD -s 192.168.55.0/24 -i ppp+ -o "$WAN_IFACE" -j ACCEPT 2>/dev/null || true
    iptables -D FORWARD -d 192.168.55.0/24 -i "$WAN_IFACE" -o ppp+ -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT 2>/dev/null || true
    if command -v firewall-cmd &>/dev/null; then
        firewall-cmd --quiet --zone=trusted --remove-interface=ppp0 2>/dev/null || true
    fi
    echo -e "${GREEN}[+] Teardown complete.${NC}"
}
trap cleanup EXIT INT TERM

# 6. Enable IP forwarding on host
echo -e "${CYAN}[*] Enabling IPv4 forwarding...${NC}"
sysctl -w net.ipv4.ip_forward=1 >/dev/null

# 7. Configure iptables NAT masquerade
echo -e "${CYAN}[*] Setting up iptables NAT rules for 192.168.55.0/24 -> ${WAN_IFACE}...${NC}"
iptables -t nat -A POSTROUTING -s 192.168.55.0/24 -o "$WAN_IFACE" -j MASQUERADE
iptables -A FORWARD -s 192.168.55.0/24 -i ppp+ -o "$WAN_IFACE" -j ACCEPT
iptables -A FORWARD -d 192.168.55.0/24 -i "$WAN_IFACE" -o ppp+ -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT

# The media app connects to the host over ppp0. Firewalld otherwise assigns
# the transient interface to its default zone and drops TCP port 5000.
if command -v firewall-cmd &>/dev/null; then
    firewall-cmd --quiet --zone=trusted --add-interface=ppp0
fi

# 8. Start pppd with Windows CE ActiveSync handshake
echo -e "${GREEN}[+] Starting PPP daemon with ActiveSync handshake...${NC}"
echo -e "    Host IP:    ${HOST_IP}"
echo -e "    WinCE IP:   ${CLIENT_IP}"
echo -e "    DNS:        ${DNS_SERVER}"
echo -e "${CYAN}[*] Once connected, open \\Windows\\iexplore.exe on the GPS to browse HTTP sites.${NC}"
echo -e "${YELLOW}[*] Press Ctrl+C to stop.${NC}"

pppd "$SERIAL_DEV" 115200 \
    noauth \
    local \
    crtscts \
    lock \
    passive \
    persist \
    maxfail 0 \
    "${HOST_IP}:${CLIENT_IP}" \
    ms-dns "$DNS_SERVER" \
    proxyarp \
    ktune \
    nodetach \
    connect "/usr/bin/chat -v -t 30 'CLIENT' 'CLIENTSERVER\\c'"
