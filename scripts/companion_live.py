"""Authenticated Bluetooth telemetry and strictly targeted YouTube Music controls.

The agent pulls snapshots; touch commands travel back through the same FTP link.
State is ephemeral on CE. Only command receipts are persisted on the PC.
"""
import base64
import hashlib
import hmac
import json
import os
from pathlib import Path
import re
import secrets
import subprocess
import time
import zlib
from urllib.parse import parse_qs, urlsplit

DEVICE = "00:18:E4:1F:03:AF"
SHARE = Path.home() / ".local/share/mero-foston-bluetooth"
STATE = Path.home() / ".config/mero/foston-agent"


def atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + ".tmp")
    temp.write_bytes(data)
    os.replace(temp, path)


def signed(fields, key):
    body = "".join(f"{k}={v}\n" for k, v in fields.items()).encode("ascii")
    return body + b"mac=" + hmac.new(key, body, hashlib.sha256).hexdigest().encode() + b"\n"


def verified(raw, key):
    if not 69 <= len(raw) <= 2048 or not raw.endswith(b"\n"):
        raise ValueError("invalid event size")
    body, tag = raw[:-69], raw[-69:]
    if tag != b"mac=" + hmac.new(key, body, hashlib.sha256).hexdigest().encode() + b"\n":
        raise ValueError("invalid event MAC")
    fields = {}
    for line in body.decode("ascii").splitlines():
        k, v = line.split("=", 1)
        if k in fields:
            raise ValueError("duplicate field")
        fields[k] = v
    if fields.get("v") != "1" or fields.get("device") != DEVICE:
        raise ValueError("wrong device")
    return fields


def run(args):
    return subprocess.run(args, capture_output=True, text=True, timeout=2, check=True).stdout.strip()


def player_state(player):
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,160}", player):
        raise ValueError("invalid player")
    payload = json.loads(run(["busctl", "--user", "--json=short", "call",
                              "org.mpris.MediaPlayer2." + player,
                              "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
                              "GetAll", "s", "org.mpris.MediaPlayer2.Player"]))["data"][0]
    get = lambda obj, name, default=None: obj.get(name, {}).get("data", default)
    meta = get(payload, "Metadata", {})
    url = urlsplit(get(meta, "xesam:url", ""))
    if url.scheme != "https" or url.hostname != "music.youtube.com":
        return None
    track = parse_qs(url.query).get("v", [""])[0]
    if not re.fullmatch(r"[A-Za-z0-9_-]{11}", track):
        return None
    artists = get(meta, "xesam:artist", [])
    return dict(player=player, track=track, trackid=get(meta, "mpris:trackid", ""),
                title=get(meta, "xesam:title", "YouTube Music"), artist=", ".join(artists),
                art_url=get(meta, "mpris:artUrl", ""),
                status=get(payload, "PlaybackStatus", "Stopped").upper(),
                pos=max(0, int(get(payload, "Position", 0)) // 1000000),
                length=max(0, int(get(meta, "mpris:length", 0)) // 1000000),
                next=bool(get(payload, "CanGoNext", False)),
                previous=bool(get(payload, "CanGoPrevious", False)),
                playpause=bool(get(payload, "CanPlay", False) and get(payload, "CanPause", False)),
                seek=bool(get(payload, "CanSeek", False)))


def music_state():
    try:
        states = []
        for player in run(["playerctl", "-l"]).splitlines()[:16]:
            try:
                state = player_state(player)
                if state:
                    states.append(state)
            except (ValueError, KeyError, subprocess.SubprocessError):
                continue
        # Do not silently choose between two active music sessions.
        playing = [s for s in states if s["status"] == "PLAYING"]
        if len(playing) == 1:
            return playing[0]
        return states[0] if len(states) == 1 else None
    except (OSError, subprocess.SubprocessError):
        return None


def music_line(state):
    clean = lambda value: str(value).replace("|", "/").replace("\n", " ").replace("\r", " ")[:160]
    fields = dict(status="UNAVAILABLE", title="Open YouTube Music on the PC", artist="", track="", pos=0, len=0,
                  next=0, previous=0, playpause=0, seek=0, art="")
    if state:
        for key in fields:
            fields[key] = state.get("length" if key == "len" else key, fields[key])
            if isinstance(fields[key], bool):
                fields[key] = int(fields[key])
    return "MERO:NOW:" + "|".join(f"{k}={clean(v)}" for k, v in fields.items())


class BluetoothLive:
    def __init__(self, share=SHARE, state=STATE):
        self.share, self.state = Path(share), Path(state)
        self.key = None
        self.receipts = {}
        self.ack = self.result = ""
        self.serial = time.time_ns()
        self.last_event = None
        self.last_publish = 0
        self.codec_offer=secrets.token_hex(8)
        try:
            self.receipts = json.loads((self.state / "live-receipts.json").read_text())
            self.ack, self.result = next(reversed(self.receipts.items()), ("", ""))
        except (OSError, ValueError):
            pass

    def load_key(self):
        try:
            # Existing agent key is private; never provision a second identity.
            self.key = bytes.fromhex((self.state / "key").read_text().strip())
            if len(self.key) != 32:
                self.key = None
        except OSError:
            self.key = None
        return self.key is not None

    def event(self):
        try:
            raw = (self.share / "live-event.txt").read_bytes()
            digest = hashlib.sha256(raw).digest()
            if digest == self.last_event:
                return
            event = verified(raw, self.key)
            identifier = event["id"]
            if not re.fullmatch(r"[0-9-]{1,79}", identifier):
                return
            self.last_event = digest
            self.ack = identifier
            if identifier in self.receipts:
                self.result = self.receipts[identifier]
                return
            # Record BEFORE execution: a lost acknowledgement never repeats Next.
            self.result = "interrupted_check_player"
            self.receipts[identifier] = self.result
            self.receipts = dict(list(self.receipts.items())[-128:])
            atomic(self.state / "live-receipts.json", json.dumps(self.receipts).encode())
            if not -3 <= time.time() - int(event["created"]) <= 30:
                self.result = "expired_press_again"
            elif event["action"].startswith("ambient-"):
                self.result = self.ambient.action(event) if getattr(self, "ambient", None) else "control_unavailable"
            elif event["action"].startswith("terminal-"):
                self.result = self.terminal.action(event) if getattr(self, "terminal", None) else "control_unavailable"
            else:
                state = music_state()
                action = event["action"]
                if not state or state["track"] != event["track"]:
                    self.result = "track_changed_press_again"
                elif action not in ("next", "previous", "playpause", "seek") or not state.get(action):
                    self.result = "control_unavailable"
                else:
                    args = ["busctl", "--user", "call", "org.mpris.MediaPlayer2." + state["player"],
                            "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player"]
                    if action == "seek":
                        position = int(event["value"])
                        if not state["length"] or not 0 <= position <= state["length"]:
                            raise ValueError("invalid seek")
                        args += ["SetPosition", "ox", state["trackid"], str(position * 1000000)]
                    else:
                        args += [{"next": "Next", "previous": "Previous", "playpause": "PlayPause"}[action]]
                    run(args)
                    self.result = "ok"
            self.receipts[identifier] = self.result
            atomic(self.state / "live-receipts.json", json.dumps(self.receipts).encode())
        except (OSError, ValueError, KeyError, subprocess.SubprocessError):
            # Malformed/untrusted events cannot perform actions.
            if self.ack and self.result == "interrupted_check_player":
                self.result = "control_failed_press_again"
                self.receipts[self.ack] = self.result
                atomic(self.state / "live-receipts.json", json.dumps(self.receipts).encode())

    def publish(self, lines, art="", art_track="", ambient="", keyboard=""):
        if not self.key and not self.load_key():
            return
        self.event()
        self.serial += 1
        # Capture terminal output after executing the just-arrived input, so
        # the echo need not wait an entire additional publication interval.
        rendered = [line() if callable(line) else line for line in lines]
        payload = ("\n".join(rendered) + "\n").encode("utf-8")
        if len(payload) >= 4096:
            # Keep the complete terminal in its dedicated compact feed. A
            # symbol-heavy screen must not crowd weather/music out of the bus.
            payload=("\n".join(line for line in rendered if not line.startswith("MERO:TERM:"))+"\n").encode("utf-8")
            if len(payload)>=4096:raise ValueError("telemetry too large")
        codec=""
        try:
            capability=verified((self.share/"live-codec.txt").read_bytes(),self.key)
            if capability.get('offer')==self.codec_offer and capability.get('codec')=='zlib1' and -3<=time.time()-int(capability['epoch'])<=8:
                if getattr(self,'terminal',None) is not None and capability.get('terminal') in ('0','1'):
                    self.terminal.demand_until=time.monotonic()+6 if capability.get('terminal')=='1' else 0
                    age=max(0,time.time()-int(capability['epoch']))
                    lease=time.monotonic()+max(0,8-age)
                    self.terminal.device_until=lease
                    self.terminal.foreground_until=lease if capability['terminal']=='1' else 0
                packed=zlib.compress(payload,1)
                if len(packed)<len(payload):payload=packed;codec='zlib1'
        except (OSError,ValueError,KeyError):pass
        job_seq=""
        try:
            job=verified((self.share/'agent-job.txt').read_bytes(),self.key)
            value=job['seq']
            if re.fullmatch(r'[0-9]{1,10}',value) and int(value)<=4294967295:job_seq=value
        except (OSError,ValueError,KeyError):pass
        fields = dict(v=1, device=DEVICE, serial=self.serial, epoch=int(time.time()),
                      ack=self.ack, result=self.result, art=art, art_track=art_track, ambient=ambient, keyboard=keyboard,
                      codec_offer=self.codec_offer,codec=codec,job_seq=job_seq,
                      data=base64.b64encode(payload).decode())
        terminal_lines = [line for line in rendered if line.startswith("MERO:TERM:")]
        fields["terminal_feed"] = int(bool(terminal_lines))
        atomic(self.share / "live-state.txt", signed(fields, self.key))
        if terminal_lines:
            # The foreground terminal needs its text, cinema state and orientation,
            # not gallery folders, weather or album metadata on every refresh.
            compact = [line for line in rendered if line.startswith("MERO:POWER:")]
            for line in rendered:
                if line.startswith("MERO:AMB:"):
                    match = re.search(r"(?:^|\|)portrait=([01])(?:\||$)", line[9:])
                    if match:compact.append("MERO:AMB:portrait=" + match[1])
            compact += terminal_lines
            compact_payload = ("\n".join(compact) + "\n").encode("utf-8")
            compact_codec = ""
            if codec:
                packed = zlib.compress(compact_payload, 1)
                if len(packed) < len(compact_payload):compact_payload=packed;compact_codec="zlib1"
            compact_fields = dict(fields, data=base64.b64encode(compact_payload).decode(),
                                  codec=compact_codec, art="", art_track="", ambient="")
            atomic(self.share / "live-terminal.txt", signed(compact_fields, self.key))
