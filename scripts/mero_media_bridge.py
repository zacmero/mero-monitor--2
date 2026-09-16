#!/usr/bin/env python3
"""Forwarder to unified host bridge"""
import os, sys
os.execv(sys.executable, [sys.executable, os.path.join(os.path.dirname(__file__), "mero_bridge.py")] + sys.argv[1:])
