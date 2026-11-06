#!/usr/bin/env python3
"""Reconnect PPP only to the Foston ActiveSync USB identity. No ROM writes or NAT."""
import signal
import shutil
import subprocess
import time
from pathlib import Path

RULE = ('rule family="ipv4" source address="192.168.55.100/32" '
        'destination address="192.168.55.101/32" '
        'port port="5000" protocol="tcp" accept')


def find_device(sys_tty=Path("/sys/class/tty"), dev_root=Path("/dev")):
    for tty in sorted(sys_tty.glob("ttyUSB*")):
        device = (tty / "device").resolve()
        if (device / "driver").resolve().name != "ipaq":
            continue
        for parent in (device, *device.parents):
            vendor, product = parent / "idVendor", parent / "idProduct"
            try:
                if vendor.read_text().strip() == "045e" and product.read_text().strip() == "00ce":
                    return dev_root / tty.name
            except OSError:
                continue
    return None


def main():
    stopping = False
    child = None
    added_rule = False

    def stop(_signum, _frame):
        nonlocal stopping
        stopping = True
        if child is not None:
            child.terminate()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    pppd, chat = shutil.which("pppd"), shutil.which("chat")
    if not pppd or not chat:
        raise SystemExit("Install pppd and chat before starting mero-sync.service")
    firewall = shutil.which("firewall-cmd")
    try:
        if firewall and subprocess.run([firewall, "--state"], capture_output=True).returncode == 0:
            if subprocess.run([firewall, "--query-rich-rule", RULE], capture_output=True).returncode != 0:
                subprocess.run([firewall, "--add-rich-rule", RULE], check=True)
                added_rule = True
        while not stopping:
            device = find_device()
            if device is None:
                time.sleep(1)
                continue
            print(f"Starting Foston PPP on {device}", flush=True)
            child = subprocess.Popen([
                pppd, str(device), "115200", "noauth", "local", "lock", "passive",
                "192.168.55.101:192.168.55.100", "nodetach", "nodefaultroute",
                "noproxyarp", "lcp-echo-interval", "5", "lcp-echo-failure", "3",
                "connect", f"{chat} -t 30 'CLIENT' 'CLIENTSERVER\\c'",
            ])
            child.wait()
            child = None
            if not stopping:
                time.sleep(2)
    finally:
        if added_rule:
            subprocess.run([firewall, "--remove-rich-rule", RULE], check=False)


if __name__ == "__main__":
    main()
