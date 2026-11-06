#!/usr/bin/env python3
"""Signed, serialized Foston jobs over the existing Bluetooth FTP share."""
import argparse
import contextlib
import fcntl
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
DEVICE = "00:18:E4:1F:03:AF"
LIMIT = 16 * 1024 * 1024


def atomic(path, data, mode=0o600):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".new")
    fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, mode)
    try:
        os.fchmod(fd, mode)
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(tmp, path)
    finally:
        if tmp.exists():
            tmp.unlink()


def sign(fields, key):
    values = []
    for name, value in fields.items():
        value = str(value)
        if not re.fullmatch(r"[a-z0-9_]+", name) or any(ord(c) < 32 or ord(c) > 126 for c in value):
            raise ValueError("Protocol fields must be single-line ASCII")
        values.append(f"{name}={value}\n")
    body = "".join(values).encode("ascii")
    return body + b"mac=" + hmac.new(key, body, hashlib.sha256).hexdigest().encode() + b"\n"


def verify(raw, key):
    if len(raw) > 2048 or not raw.endswith(b"\n") or len(raw) < 69:
        raise ValueError("Incomplete agent message")
    body, macline = raw[:-69], raw[-69:]
    expected = b"mac=" + hmac.new(key, body, hashlib.sha256).hexdigest().encode() + b"\n"
    if not hmac.compare_digest(expected, macline):
        raise ValueError("Invalid agent signature")
    fields = {}
    for line in body.decode("ascii").splitlines():
        name, sep, value = line.partition("=")
        if not sep or name in fields or not re.fullmatch(r"[a-z0-9_]+", name):
            raise ValueError("Malformed agent fields")
        fields[name] = value
    return fields


def device_path(value, write=False):
    path = value.replace("/", "\\")
    if not path.startswith("\\") or len(path) > 180 or any(ord(c) < 32 or ord(c) > 126 or c in ':*?"<>|' for c in path):
        raise ValueError("Use an absolute ASCII WinCE path")
    if any(part in ("", ".", "..") for part in path[1:].split("\\")):
        raise ValueError("Empty/path-traversal components are forbidden")
    low = path.lower()
    roots = ("\\sdmmc", "\\windows", "\\residentflash", "\\residentflash2")
    if not write:
        if not any(low == root or low.startswith(root + "\\") for root in roots):
            raise ValueError("Read outside device storage roots")
    else:
        if not any(low.startswith("\\sdmmc\\" + root + "\\") for root in ("mero", "bt", "photos", "stream")):
            raise ValueError("Writes allowed only inside SDMMC/MERO, BT, Photos, Stream")
        if any(low.startswith("\\sdmmc\\mero\\" + root) for root in ("agent-", "deploy-backups\\", "deploy-staging\\")) or low in ("\\sdmmc\\mero\\deploy-backups", "\\sdmmc\\mero\\deploy-staging"):
            raise ValueError("Agent state and recovery backups are protected")
    return path


def app_name(value):
    if not re.fullmatch(r"mero-[a-z0-9.-]+\.exe", value) or not 10 <= len(value) <= 63:
        raise ValueError("Only named mero-*.exe applications can be controlled")
    return value


def read_payload(path):
    with Path(path).open("rb") as stream:
        data = stream.read(LIMIT + 1)
    if len(data) > LIMIT:
        raise ValueError("File exceeds 16 MiB agent limit")
    return data


class Client:
    def __init__(self, state, share, timeout=60):
        self.state, self.share, self.timeout = Path(state), Path(share), timeout
        self.state.mkdir(mode=0o700, parents=True, exist_ok=True)
        self.state.chmod(0o700)
        self.share.mkdir(parents=True, exist_ok=True)

    @contextlib.contextmanager
    def locked(self):
        with (self.state / "lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            yield

    def key(self):
        try:
            key = bytes.fromhex((self.state / "key").read_text().strip())
        except FileNotFoundError:
            raise RuntimeError("Run bootstrap first; no device key is provisioned") from None
        if len(key) != 32:
            raise RuntimeError("Invalid local key; do not replace an existing device key")
        return key

    def bootstrap(self, adapter=None):
        keyfile = self.state / "key"
        if not keyfile.exists():
            atomic(keyfile, secrets.token_hex(32).encode() + b"\n")
        key = self.key()
        if adapter is None:
            output = subprocess.check_output(["bluetoothctl", "show"], text=True)
            match = re.search(r"Controller ([0-9A-Fa-f:]{17})", output)
            if not match:
                raise RuntimeError("No Bluetooth adapter found")
            adapter = match.group(1)
        if not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", adapter):
            raise ValueError("Invalid Bluetooth adapter address")
        peer = list(reversed(bytes.fromhex(adapter.replace(":", "")))) + [0, 0]
        header = (f'#define MERO_AGENT_KEY_HEX "{key.hex()}"\n'
                  f'#define MERO_AGENT_DEVICE "{DEVICE}"\n'
                  '#define MERO_AGENT_HOST_BYTES ' + ','.join(f'0x{x:02x}' for x in peer) + '\n')
        atomic(ROOT / "wince/bluetooth/mero-agent-key.h", header.encode())
        subprocess.run(["make", "-C", str(ROOT / "wince/bluetooth"), "agent"], check=True, stdout=sys.stderr)
        built = ROOT / "wince/bluetooth/mero-agent.exe"
        from deploy_sd import verify_binary
        verify_binary(built)
        private = self.state / "mero-agent.exe"
        shutil.move(str(built), private)
        atomic(self.share / "mero-agent.exe", private.read_bytes(), 0o600)
        if not (self.state / "pending.json").exists():
            atomic(self.share / "agent-job.txt", sign({"v": 1, "device": DEVICE, "seq": 0, "op": "idle"}, key), 0o600)
        return {"ok": True, "bootstrap": str(self.share / "mero-agent.exe"),
                "sha256": hashlib.sha256(private.read_bytes()).hexdigest(),
                "next": "Download mero-agent.exe over Bluetooth and run it. Agent mode starts automatically. Then run status."}

    def rpc(self, op, path="", payload=None, ref=0, output=None, args=""):
        if (self.state / "pending.json").exists():
            raise RuntimeError("Previous job unresolved. Run wait before issuing another command")
        key = self.key()
        counter = self.state / "sequence"
        seq = int(counter.read_text()) + 1 if counter.exists() else 1
        if seq > 0xffffffff:
            raise RuntimeError("Sequence exhausted; reprovision through a separate recovery workflow")
        atomic(counter, str(seq).encode())
        if payload is not None:
            if len(payload) > LIMIT:
                raise ValueError("File exceeds 16 MiB agent limit")
            atomic(self.share / "agent-payload.bin", payload, 0o600)
        fields = {"v": 1, "device": DEVICE, "seq": seq, "op": op, "path": path, "args": args,
                  "size": len(payload) if payload is not None else 0,
                  "sha256": hashlib.sha256(payload).hexdigest() if payload is not None else "",
                  "ref": ref}
        pending = {"seq": str(seq), "op": op, "path": path, "output": str(Path(output).resolve()) if output else None}
        atomic(self.state / "pending.json", json.dumps(pending).encode())
        atomic(self.share / "agent-job.txt", sign(fields, key), 0o600)
        return self.wait()

    def wait(self):
        pendingfile = self.state / "pending.json"
        if not pendingfile.exists():
            raise RuntimeError("No pending job")
        pending = json.loads(pendingfile.read_text())
        deadline = time.monotonic() + self.timeout
        result = None
        while time.monotonic() < deadline:
            try:
                with (self.share / "agent-result.txt").open("rb") as stream:
                    candidate = verify(stream.read(2049), self.key())
                if candidate.get("v") == "1" and candidate.get("seq") == pending["seq"] and candidate.get("op") == pending["op"] and candidate.get("device") == DEVICE:
                    result = candidate
                    break
            except (OSError, ValueError, UnicodeError):
                pass
            time.sleep(0.2)
        if result is None:
            raise RuntimeError(f'No authenticated reply for job {pending["seq"]}; leave it pending and use wait after reconnecting the device')
        receipt = self.state / "jobs" / pending["seq"]
        receipt.mkdir(parents=True, exist_ok=True)
        if result.get("ok") == "1" and pending["op"] in ("get", "ls"):
            data = read_payload(self.share / "agent-output.bin")
            if len(data) > LIMIT or str(len(data)) != result.get("size") or hashlib.sha256(data).hexdigest() != result.get("sha256"):
                raise RuntimeError("Reply data hash mismatch; leave job pending and retry wait")
            atomic(receipt / "data.bin", data)
            if pending["output"]:
                dest = Path(pending["output"])
                if dest.exists():
                    shutil.copy2(dest, receipt / "previous-local-file")
                atomic(dest, data)
                result["local_file"] = str(dest)
            elif pending["op"] == "ls":
                result["listing"] = data.decode("utf-8")
        atomic(receipt / "result.json", json.dumps(result, indent=2).encode())
        # Retire the command after receiving its authenticated outcome. A device
        # reboot must not republish old retrieved data into the public share.
        atomic(self.share / "agent-job.txt", sign({"v": 1, "device": DEVICE, "seq": 0, "op": "idle"}, self.key()), 0o600)
        pendingfile.unlink()
        if pending["op"] == "put" and (self.share / "agent-payload.bin").exists():
            os.replace(self.share / "agent-payload.bin", receipt / "input.bin")
        if pending["op"] in ("get", "ls") and (self.share / "agent-output.bin").exists():
            (self.share / "agent-output.bin").unlink()
        public = self.share / "mero-agent.exe"
        if public.exists():
            private = self.state / "bootstrap-archives" / (str(time.time_ns()) + ".exe")
            private.parent.mkdir(parents=True, exist_ok=True)
            os.replace(public, private)
        if result.get("ok") != "1":
            raise RuntimeError(f'Device rejected job {pending["seq"]}: {result.get("message", "unknown error")}; receipt: {receipt}')
        return result

    def cancel_read(self):
        pendingfile = self.state / "pending.json"
        if not pendingfile.exists():
            raise RuntimeError("No pending job")
        pending = json.loads(pendingfile.read_text())
        if pending["op"] not in ("status", "ls", "get"):
            raise RuntimeError("Cannot cancel an uncertain mutation; use wait and inspect its receipt")
        archive = self.state / "jobs" / pending["seq"]
        archive.mkdir(parents=True, exist_ok=True)
        atomic(archive / "cancelled-read.json", json.dumps(pending).encode())
        atomic(self.share / "agent-job.txt", sign({"v": 1, "device": DEVICE, "seq": 0, "op": "idle"}, self.key()), 0o600)
        pendingfile.unlink()
        return {"ok": True, "cancelled_read": pending["seq"], "note": "No device mutation was cancelled or replayed"}

    def upgrade(self, target=None):
        """Separate, hash-bound helper replaces the agent at its existing path."""
        current = self.rpc("status")
        if target is None:
            target = current.get("agent_path")
        if target is None:
            listing = self.rpc("ls", "\\SDMMC\\BT\\Downloads")["listing"]
            names = [line.split("\t", 2)[2] for line in listing.splitlines()
                     if line.startswith("file\t") and line.endswith("mero-agent.exe") and not line.split("\t", 2)[2].startswith("partial-")]
            if len(names) != 1:
                raise RuntimeError("Cannot identify running agent path; use upgrade --target with its navigation path")
            target = "\\SDMMC\\BT\\Downloads\\" + names[0]
        target = device_path(target, True)
        old = self.state / "mero-agent.exe"
        oldsha = current.get("agent_sha256") or hashlib.sha256(old.read_bytes()).hexdigest()
        if not (ROOT / "wince/bluetooth/mero-agent-key.h").exists():
            raise RuntimeError("Run bootstrap to restore the ignored build header using the existing key; do not replace the key")
        subprocess.run(["make", "-C", str(ROOT/"wince/bluetooth"), "agent"], check=True, stdout=sys.stderr)
        built = ROOT / "wince/bluetooth/mero-agent.exe"
        from deploy_sd import verify_binary
        verify_binary(built)
        nextfile = self.state / "next-agent.exe"
        shutil.move(str(built), nextfile)
        data = nextfile.read_bytes()
        newsha = hashlib.sha256(data).hexdigest()
        nonce = secrets.token_hex(8)
        candidate = "\\SDMMC\\BT\\AgentUpdate\\next.exe"
        backup = "\\SDMMC\\BT\\AgentUpdate\\old-" + nonce + ".exe"
        plan = (f"v=1\ntarget={target}\ncandidate={candidate}\nbackup={backup}\nold_sha256={oldsha}\nnew_sha256={newsha}\n").encode("ascii")
        header = '#define MERO_UPGRADE_PLAN_SHA256 "' + hashlib.sha256(plan).hexdigest() + '"\n'
        atomic(ROOT / "wince/bluetooth/mero-upgrade-plan.h", header.encode())
        subprocess.run(["make", "-C", str(ROOT/"wince/bluetooth"), "agent-upgrade"], check=True, stdout=sys.stderr)
        helper = ROOT / "wince/bluetooth/mero-agent-upgrade.exe"
        verify_binary(helper)
        privatehelper = self.state / "mero-agent-upgrade.exe"
        shutil.move(str(helper), privatehelper)
        self.rpc("put", candidate, payload=data)
        self.rpc("put", "\\SDMMC\\BT\\AgentUpdate\\plan.txt", payload=plan)
        self.rpc("put", "\\SDMMC\\MERO\\mero-agent-upgrade.exe", payload=privatehelper.read_bytes())
        self.rpc("launch", "mero-agent-upgrade.exe")
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            latest = self.rpc("status")
            if latest.get("agent_sha256") == newsha:
                if old.exists():
                    versions = self.state / "versions"
                    versions.mkdir(exist_ok=True)
                    shutil.copy2(old, versions / (hashlib.sha256(old.read_bytes()).hexdigest()+".exe"))
                os.replace(nextfile, old)
                return {"ok": True, "agent_path": target, "sha256": newsha,
                        "device_backup": backup, "status": latest}
            time.sleep(0.5)
        raise RuntimeError("Upgrade not verified; inspect BT/AgentUpdate/upgrade-result.txt before retrying")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-dir", type=Path, default=Path.home()/".config/mero/foston-agent")
    parser.add_argument("--share", type=Path, default=Path.home()/".local/share/mero-foston-bluetooth")
    parser.add_argument("--timeout", type=float, default=60)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("bootstrap"); p.add_argument("--adapter")
    p = sub.add_parser("upgrade"); p.add_argument("--target")
    sub.add_parser("status"); sub.add_parser("wait"); sub.add_parser("cancel-read")
    p = sub.add_parser("ls"); p.add_argument("path", nargs="?", default="/SDMMC/MERO")
    p = sub.add_parser("get"); p.add_argument("path"); p.add_argument("output", type=Path)
    p = sub.add_parser("put"); p.add_argument("source", type=Path); p.add_argument("path")
    for name in ("launch", "stop"):
        p = sub.add_parser(name); p.add_argument("name")
        if name == "launch":
            p.add_argument("--view", choices=("weather","fm","gps","clock"))
            p.add_argument("--home", action="store_true")
    p = sub.add_parser("rollback"); p.add_argument("reference", type=int)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    client = Client(args.state_dir, args.share, args.timeout)
    try:
        with client.locked():
            if args.command == "bootstrap": result = client.bootstrap(args.adapter)
            elif args.command == "upgrade": result = client.upgrade(args.target)
            elif args.command == "wait": result = client.wait()
            elif args.command == "cancel-read": result = client.cancel_read()
            elif args.command == "status": result = client.rpc("status")
            elif args.command == "ls": result = client.rpc("ls", device_path(args.path))
            elif args.command == "get": result = client.rpc("get", device_path(args.path), output=args.output)
            elif args.command == "put": result = client.rpc("put", device_path(args.path, True), payload=read_payload(args.source))
            elif args.command in ("launch", "stop"):
                launch_args = ""
                if args.command == "launch":
                    if args.view:
                        if args.name != "mero-media-ctrl.exe": raise ValueError("--view requires mero-media-ctrl.exe")
                        launch_args = "--" + args.view
                    if args.home:
                        if args.name != "mero-shell.exe": raise ValueError("--home requires mero-shell.exe")
                        launch_args = "--home"
                result = client.rpc(args.command, app_name(args.name), args=launch_args)
            else:
                if not 1 <= args.reference <= 0xffffffff: raise ValueError("Invalid rollback reference")
                result = client.rpc("rollback", ref=args.reference)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(json.dumps({"ok": False, "error": str(error)}), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
