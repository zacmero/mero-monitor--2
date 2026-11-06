#!/usr/bin/env python3
"""Stage, verify, and back up app binaries on the known Foston SD card."""
import hashlib
import os
import shutil
import struct
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UUID = "6232-3562"


def verify_binary(path):
    data = path.read_bytes()
    if len(data) < 256 or len(data) > 2 * 1024 * 1024 or data[:2] != b"MZ":
        raise ValueError(f"Invalid executable: {path}")
    pe = struct.unpack_from("<I", data, 60)[0]
    if pe + 96 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError(f"Invalid PE header: {path}")
    machine = struct.unpack_from("<H", data, pe + 4)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    subsystem = struct.unpack_from("<H", data, pe + 24 + 68)[0]
    if machine not in (0x1C0, 0x1C2) or magic != 0x10B or subsystem not in (2, 9):
        raise ValueError(f"Not a WinCE ARM GUI executable: {path}")


def deploy(mount):
    mount = Path(mount)
    if not mount.is_mount():
        raise SystemExit(f"Not a mounted SD card: {mount}")
    detected = subprocess.check_output(
        ["findmnt", "-n", "-o", "UUID", "--target", str(mount)], text=True).strip()
    if detected.upper() != UUID:
        raise SystemExit(f"Refusing unknown filesystem UUID {detected!r}")
    sources = sorted((ROOT / "wince").glob("*/*.exe"))
    required = {"mero-shell.exe", "mero-gallery.exe", "mero-media-ctrl.exe", "mero-tuner.exe", "mero-cam.exe", "mero-flash.exe", "mero-bt.exe"}
    if not required.issubset({source.name for source in sources}):
        raise SystemExit("Build shell, gallery, media, tuner, cam, flash, and Bluetooth before deploying")
    for source in sources:
        verify_binary(source)
    stamp = time.strftime("%Y%m%d-%H%M%S") + f"-{os.getpid()}"
    mero = mount / "MERO"
    stage = mero / "deploy-staging" / stamp
    backup = mero / "deploy-backups" / stamp
    stage.mkdir(parents=True)
    backup.mkdir(parents=True)
    # Validate every staged copy before replacing any installed executable.
    for source in sources:
        target = stage / source.name
        shutil.copy2(source, target)
        if hashlib.sha256(target.read_bytes()).digest() != hashlib.sha256(source.read_bytes()).digest():
            raise RuntimeError(f"Staged copy differs: {target}")
        if (mero / source.name).exists():
            shutil.copy2(mero / source.name, backup / source.name)
            if (backup / source.name).read_bytes() != (mero / source.name).read_bytes():
                raise RuntimeError(f"Backup differs: {source.name}")
    os.sync()
    for source in sources:
        os.replace(stage / source.name, mero / source.name)
    os.sync()
    print(f"Apps deployed. Previous binaries: {backup}")
    print("Factory files, boot registry, internal flash, and app configs were preserved.")


if __name__ == "__main__":
    import sys
    deploy(sys.argv[1] if len(sys.argv) > 1 else "/run/media/zacmero/6232-3562")
