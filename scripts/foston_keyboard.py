#!/usr/bin/env python3
"""Diagnostic socket client; normal keyboard forwarding runs inside mero-bridge."""
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
