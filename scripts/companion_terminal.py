"""Persistent, bounded tmux terminal and explicit local keyboard forwarding.

No command strings from the wire are passed to a shell by this module. Input
goes to the terminal PTY as bytes, just like an attached keyboard.
"""
import base64
import json
import os
from pathlib import Path
import re
import secrets
import socket
import subprocess
import time
import unicodedata

from companion_live import atomic, verified
from companion_keyboard import Keyboard, KeysHeld

TARGETS = {"terminal": "MERO_TERMINAL", "shell": "MeroShellWndClass",
           "ambient": "MERO_AMBIENT", "backlight": "MERO_BACKLIGHT",
           "music": "MERO_MUSIC"}


def terminal_cells(row,cols):
    """One UTF-16 code unit per terminal cell, including wide-cell continuation."""
    cells=[]
    for char in unicodedata.normalize("NFC",row):
        if unicodedata.combining(char) or unicodedata.category(char) in ("Cf","Mn","Me"):continue
        width=1 if 0xf0000<=ord(char)<=0xf2fff else 2 if unicodedata.east_asian_width(char) in ("W","F") else 1
        if len(cells)+width>cols:break
        code=ord(char)
        if code<32 or code==127:char=" "
        elif 0xf0000<=code<=0xf2fff:
            # Reserved wire cells keep supplementary Nerd icons at two bytes.
            char=chr(code-0xf0000+0xa000);width=1
        elif code>0xffff or 0xa000<=code<=0xcfff or 0xd800<=code<=0xdfff:char="□"
        cells.append(char)
        if width==2:cells.append("\0")
    return "".join(cells)


def socket_path():
    return Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "mero-foston-input.sock"


def fastfetch_config():
    return {"logo": {"type": "data", "source": "  /\\\n /__\\\n/    \\", "padding": {"right": 1}},
            "display": {"separator": ": "},
            "modules": ["title", {"type": "os", "key": "OS", "format": "ArchMerOS {arch}"},
                        {"type": "kernel", "key": "Krn", "format": "{release}"},
                        {"type": "shell", "key": "Sh", "format": "{pretty-name}"},
                        {"type": "wm", "key": "WM", "format": "{pretty-name}"},
                        {"type": "uptime", "key": "Up", "format": "{hours}h {minutes}m"},
                        {"type": "memory", "key": "RAM", "format": "{used}"}]}


class Terminal:
    def __init__(self, share, key_provider, state=None, endpoint=None, server="mero-foston", managed=False):
        self.share, self.key_provider = Path(share), key_provider
        self.state = Path(state or Path.home()/".local/state/mero/foston-terminal")
        self.state.mkdir(parents=True,exist_ok=True,mode=0o700)
        atomic(self.state/"fastfetch.json",json.dumps(fastfetch_config()).encode())
        self.endpoint = Path(endpoint or socket_path())
        self.server = server
        self.managed = managed
        self.token = secrets.token_hex(8)
        self.cols, self.rows, self.scroll = 32, 26, 0
        self.active = False
        self.physical = False
        self.arming_until = 0
        self.next_arm = 0
        self.device_until = 0
        self.foreground_until = 0
        self.keyboard_target = 'MERO_FOREGROUND'
        self.keyboard_owner = ""
        self.keyboard_deadline = 0
        self.capture=Keyboard()
        self.keyboard_notice=""
        self.pending = None
        self.status = "Open terminal"
        self.demand_until = 0
        self.listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # Fail rather than steal another running bridge's socket.
        if self.endpoint.exists():
            probe = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                probe.connect(str(self.endpoint))
            except ConnectionRefusedError:
                self.endpoint.unlink()
            else:
                raise RuntimeError("Foston keyboard listener already running")
            finally:
                probe.close()
        self.listener.bind(str(self.endpoint))
        os.chmod(self.endpoint, 0o600)
        self.listener.listen(1)
        self.listener.setblocking(False)
        self.client = None
        self.client_data = b""
        self.client_deadline = 0

    def tmux(self, *args):
        return subprocess.run(["tmux", "-L", self.server, "-f", "/dev/null", *args],
                              capture_output=True, timeout=1, check=True).stdout

    def start(self):
        if self.active:
            return
        try:
            self.tmux("has-session", "-t", "foston")
        except subprocess.CalledProcessError:
            self.state.mkdir(parents=True, exist_ok=True, mode=0o700)
            # Source normal Bash setup/aliases, suppress its full-size fetch,
            # then use a compact prompt and our small fastfetch configuration.
            rc = ('export MERO_FASTFETCH_SHOWN=1\n'
                  '[[ -r ~/.bashrc ]] && source ~/.bashrc\n'
                  'unset PROMPT_COMMAND\nPS1="\\[\\e[38;5;114m\\]mero:\\W\\$ \\[\\e[0m\\]"\n'
                  'printf "\\033[2J\\033[H"\n'
                  '/usr/bin/fastfetch --config "${MERO_FOSTON_STATE}/fastfetch.json"\n')
            atomic(self.state/"bashrc", rc.encode())
            # tmux's multi-argument form execs directly, without shell parsing.
            args = ["new-session", "-d", "-s", "foston", "-x", str(self.cols),
                      "-y", str(self.rows), "-c", str(Path.home()),
                      "-e", "MERO_FOSTON_STATE="+str(self.state),
                      "/bin/bash", "--rcfile", str(self.state/"bashrc"), "-i"]
            if self.managed:
                # A separate user unit survives bridge restarts/cgroup cleanup.
                subprocess.run(["systemd-run", "--user", "--unit=mero-foston-terminal", "--collect",
                                "--property=Type=forking", "--", "/usr/bin/tmux", "-L", self.server,
                                "-f", "/dev/null", *args], capture_output=True, timeout=3, check=True)
            else:
                self.tmux(*args)
            self.tmux("set-option", "-t", "foston", "status", "off")
            self.tmux("set-option", "-t", "foston", "history-limit", "1000")
        self.active = True

    def action(self, event):
        action, track = event["action"], event["track"]
        token, _, payload = track.partition(".")
        if token != self.token:
            return "session_changed_press_again"
        if action not in {"terminal-open", "terminal-input", "terminal-view", "terminal-scroll", "terminal-keyboard", "terminal-touch", "terminal-ping"}:
            return "control_unavailable"
        self.demand_until = time.monotonic()+12
        if action == "terminal-ping":
            return "ok"
        if action == "terminal-keyboard":
            self.arming_until=0;self.keyboard_target='MERO_FOREGROUND'
            try:self.capture.start()
            except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
                self.capture.close();self.physical=False
                self.keyboard_notice="PC keyboard access needed" if isinstance(error,PermissionError) else str(error)[:70]
                return "keyboard_unavailable"
            self.physical=True;self.keyboard_notice=""
            self.keyboard_deadline = time.monotonic()+15
            return "ok"
        if action == "terminal-touch":
            self.arming_until=0
            self.capture.close()
            self.keyboard_notice=""
            self.physical = False
            self.keyboard_owner = ""
            return "ok"
        if action == "terminal-input":
            if not self.active or not re.fullmatch(r"(?:[0-9a-f]{2}){1,32}", payload):
                return "invalid_terminal_input"
            self.tmux("send-keys", "-t", "foston:0.0", "-H", *[payload[i:i+2] for i in range(0,len(payload),2)])
            self.scroll = 0
        elif action == "terminal-view":
            value = int(event["value"])
            if value not in range(4):
                return "invalid_terminal_view"
            portrait, keyboard = value & 1, value & 2
            self.cols = 32 if portrait else 58
            self.rows = (16 if keyboard else 26) if portrait else (4 if keyboard else 14)
            self.start()
            self.tmux("resize-window", "-t", "foston:0", "-x", str(self.cols), "-y", str(self.rows))
            self.scroll = 0
        elif action == "terminal-scroll":
            value = int(event["value"])
            if value not in (0, 1, 2):
                return "invalid_terminal_scroll"
            self.scroll = min(1000, self.scroll+self.rows) if value == 1 else max(0,self.scroll-self.rows) if value == 2 else 0
        else:
            self.start()
        return "ok"

    def update(self):
        if self.physical and time.monotonic()>self.keyboard_deadline:
            self.capture.close()
            self.physical = False
            self.keyboard_owner = ""
        text, cx, cy = "", 0, 0
        try:
            if self.active:
                # Plain capture omits ANSI/OSC escapes. Bound every row and use
                # UTF-16 cells preserve symbols while bounding the signed 4KB feed.
                args = ["capture-pane", "-p", "-t", "foston:0.0"]
                if self.scroll:
                    args += ["-S", str(-self.scroll), "-E", str(self.rows-1-self.scroll)]
                raw = self.tmux(*args, ";", "display-message", "-p", "-t", "foston:0.0",
                                "MERO_CURSOR:#{cursor_x}:#{cursor_y}").decode("utf-8", "replace")
                raw, marker, cursor = raw.rpartition("MERO_CURSOR:")
                if not marker:raise ValueError("missing terminal cursor")
                cx, cy = map(int,cursor.strip().split(':'))
                rows = raw.splitlines()[:self.rows]
                text = "\n".join(terminal_cells(row,self.cols) for row in rows)
                self.status = "SCROLLBACK" if self.scroll else "PC SESSION"
        except (OSError, ValueError, subprocess.SubprocessError):
            self.active = False
            self.status = "Session ended / tap CONNECT"
        encoded = base64.b64encode(text.encode("utf-16le")).decode()
        return (f"MERO:TERM:token={self.token}|cols={self.cols}|rows={self.rows}|cx={cx}|cy={cy}"
                f"|scroll={self.scroll}|active={int(self.active)}|physical={int(self.physical)}|input={self.keyboard_notice}|status={self.status}|encoding=u16|text={encoded}")

    def finish_client(self, result):
        if self.client:
            try:
                self.client.sendall((result+"\n").encode())
            except OSError:
                pass
            self.client.close()
        self.client, self.client_data, self.pending = None, b"", None

    def poll_keyboard(self):
        now = time.monotonic()
        if self.pending:
            try:
                key = self.key_provider()
                reply = verified((self.share/"keyboard-result.txt").read_bytes(), key)
                if reply.get("id") == self.pending["id"]:
                    if self.capture.fds and reply.get("result")!='queued':
                        self.keyboard_notice=reply.get("result","Input outcome unknown");self.capture.close();self.physical=False
                    self.finish_client(reply.get("result", "unknown"))
            except (OSError, ValueError, TypeError):
                pass
        if (self.client or self.pending) and now > self.client_deadline:
            self.capture.close();self.physical=False;self.keyboard_notice="Input timeout - check PC prompt"
            self.finish_client("timeout_unknown_do_not_repeat")
        if self.client is None:
            try:
                self.client, _ = self.listener.accept()
                self.client.setblocking(False)
                self.client_deadline = now+12
            except BlockingIOError:
                return
        if self.pending:
            return
        try:
            chunk = self.client.recv(1025)
            if not chunk:
                self.finish_client("closed")
                return
            self.client_data += chunk
            if len(self.client_data) > 512:
                raise ValueError("oversized")
            if b"\n" not in self.client_data:
                return
            request = json.loads(self.client_data)
            mode = request.get("mode")
            if mode in ("keyboard-state", "toggle-terminal"):
                if not re.fullmatch(r"[0-9a-f]{16}",request.get("owner", "")):
                    raise ValueError("invalid keyboard owner")
                if mode == "keyboard-state":
                    self.finish_client("physical" if self.physical else "arming" if self.arming_until else "touch" if self.device_until>now else "offline")
                elif self.physical or self.arming_until:
                    self.capture.close();self.physical=False;self.arming_until=0;self.keyboard_notice="";self.keyboard_owner=""
                    self.finish_client("touch")
                elif self.device_until<=now:
                    self.finish_client("device_offline")
                else:
                    try:self.start()
                    except (OSError,RuntimeError,ValueError,subprocess.SubprocessError):
                        self.keyboard_notice="Terminal session unavailable"
                        self.finish_client("terminal_unavailable");return
                    self.arming_until=now+10;self.next_arm=0;self.keyboard_target=TARGETS['terminal']
                    self.keyboard_notice="Release shortcut keys / waiting for terminal"
                    self.finish_client("arming")
                return
            if mode in ("engage", "release", "status"):
                owner = request.get("owner", "")
                if not re.fullmatch(r"[0-9a-f]{16}",owner):
                    raise ValueError("invalid keyboard owner")
                if mode == "engage":
                    self.physical, self.keyboard_owner = True, owner
                matched = self.physical and owner == self.keyboard_owner
                if mode == "release" and matched:
                    self.capture.close();self.arming_until=0
                    self.physical, self.keyboard_owner = False, ""
                    matched = False
                if matched:
                    self.keyboard_deadline = now+15
                self.finish_client("physical" if matched else "touch")
                return
            if not self.physical:
                self.finish_client("touch_mode_selected")
                return
            self.keyboard_deadline = now+15
            target, data = request.get("target"), request.get("hex", "")
            if target not in TARGETS.values() or not re.fullmatch(r"(?:[0-9a-f]{2}){1,32}", data):
                raise ValueError("invalid input")
            self.pending = dict(id=secrets.token_hex(8), target=target, data=data)
        except BlockingIOError:
            pass
        except (OSError, ValueError, TypeError):
            self.finish_client("invalid_request")

    def keyboard_field(self):
        self.poll_keyboard()
        if self.arming_until:
            now=time.monotonic()
            if now>=self.arming_until:
                self.arming_until=0;self.keyboard_notice="Keyboard activation timed out"
            elif self.active and self.foreground_until>now and now>=self.next_arm:
                self.next_arm=now+0.15
                try:
                    self.capture.start();self.arming_until=0;self.physical=True
                    self.keyboard_notice="";self.keyboard_deadline=now+15
                except KeysHeld:
                    pass
                except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
                    self.capture.close();self.arming_until=0;self.physical=False;self.keyboard_notice=str(error)[:70]
        if self.capture.fds:
            try:
                if self.keyboard_target==TARGETS['terminal'] and self.foreground_until<=time.monotonic():
                    raise RuntimeError("Foston terminal not in front - PC keyboard released")
                self.capture.poll()
                self.keyboard_deadline=time.monotonic()+15
                if not self.pending and self.capture.queue:
                    self.pending=dict(id=secrets.token_hex(8),target=self.keyboard_target,data=self.capture.packet().hex())
                    self.client_deadline=time.monotonic()+12
            except (OSError,RuntimeError,ValueError,subprocess.SubprocessError) as error:
                self.keyboard_notice=str(error)[:70];self.capture.close();self.physical=False
        p = self.pending
        return f"{p['id']}:{p['target']}:{p['data']}" if p else ""

    def close(self):
        self.capture.close()
        self.finish_client("bridge_stopped")
        self.listener.close()
        self.endpoint.unlink(missing_ok=True)
        # Intentionally leave the isolated tmux session running.
