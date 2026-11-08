#!/usr/bin/env python3
"""Windowless terminal shortcut and diagnostic client; capture stays in mero-bridge."""
import json
import secrets
import socket
from companion_terminal import TARGETS, socket_path
OWNER=secrets.token_hex(8)


def control(mode, endpoint=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as client:
        client.settimeout(5)
        client.connect(str(endpoint or socket_path()))
        client.sendall(json.dumps({"mode":mode,"owner":OWNER}).encode()+b"\n")
        return client.recv(128).decode().strip()


def forward(data, target, endpoint=None):
    if not 1 <= len(data) <= 32:
        raise ValueError("input must contain 1-32 bytes")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(15)
        client.connect(str(endpoint or socket_path()))
        client.sendall(json.dumps({"target": TARGETS[target], "hex": data.hex()}).encode()+b"\n")
        result = client.recv(256).decode().strip()
        return result or "connection_closed_unknown"


def toggle_terminal(endpoint=None):
    """Windowless shortcut: bring up the device app, then arm after key release."""
    from pathlib import Path
    import fcntl
    import subprocess
    import sys
    lock_path=Path(endpoint or socket_path()).with_name("mero-foston-toggle.lock")
    with lock_path.open("a") as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:return "busy"
        state=control("keyboard-state",endpoint)
        if state not in ("physical","arming"):
            if state=="offline":raise RuntimeError("Foston is not connected")
            if state!="touch":raise RuntimeError("Foston keyboard bridge is unavailable or outdated")
            agent=Path(__file__).with_name("foston_agent.py")
            # Agent CLI owns durable sequence/pending-job handling. A failed or
            # unknown launch must not grab the keyboard or be retried here.
            result=subprocess.run([sys.executable,str(agent),"launch","mero-pc-terminal.exe"],
                                  capture_output=True,text=True)
            if result.returncode:raise RuntimeError("Foston terminal launch failed; inspect agent status/pending job")
        result=control("toggle-terminal",endpoint)
        if result not in ("arming","touch"):raise RuntimeError("Foston keyboard toggle failed: "+result)
        return result


if __name__=="__main__":
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action",choices=["toggle-terminal"])
    parser.parse_args()
    try:print(toggle_terminal())
    except (OSError,RuntimeError) as error:parser.exit(1,str(error)+"\n")
