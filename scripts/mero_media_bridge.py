#!/usr/bin/env python3
"""
mero_media_bridge.py - Host Bridge for Mero Monitor #2 YouTube Music Deck
Connects host media players (MPRIS / playerctl) to Windows CE companion deck.
Supports DUAL-STACK communication:
  1. USB Serial Link (/dev/ttyUSB0) for ActiveSync / Vsync mode (Instantaneous Packet Stream)
  2. Storage Mailbox (SDMMC/Stream/now_playing.txt) for Mass Storage mode (Direct I/O)
Locks onto YouTube Music instance (music.youtube.com).
"""

import os
import sys
import time
import shutil
import select
import termios
import urllib.request
import subprocess
from pathlib import Path

DEFAULT_MOUNT = Path("/run/media/zacmero/6232-3562")
DEV_UUID = "6232-3562"
SERIAL_PORT = "/dev/ttyUSB0"

last_art_url = ""
last_title = ""
last_status = ""
active_player = ""
last_processed_seq = -1
last_processed_cmd = ""
serial_fd = None
rx_serial_buf = ""
last_serial_send_time = 0.0

def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=2)
        return res.stdout.strip()
    except Exception:
        return ""

def check_serial_connection():
    """Detect, configure, and maintain connection to /dev/ttyUSB0."""
    global serial_fd
    if serial_fd is not None:
        if not os.path.exists(SERIAL_PORT):
            try:
                os.close(serial_fd)
            except Exception:
                pass
            serial_fd = None
            print("[!] Serial port /dev/ttyUSB0 disconnected")
            return None
        return serial_fd

    if os.path.exists(SERIAL_PORT):
        try:
            fd = os.open(SERIAL_PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            attrs = termios.tcgetattr(fd)
            # 115200 baud
            attrs[4] = termios.B115200
            attrs[5] = termios.B115200
            # Raw mode 8N1
            attrs[0] &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP | termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
            attrs[1] &= ~termios.OPOST
            attrs[2] &= ~(termios.CSIZE | termios.PARENB)
            attrs[2] |= termios.CS8 | termios.CLOCAL | termios.CREAD
            attrs[3] &= ~(termios.ECHO | termios.ECHONL | termios.ICANON | termios.ISIG | termios.IEXTEN)
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            serial_fd = fd
            print(f"[+] Attached to USB Serial Link at {SERIAL_PORT} (ActiveSync mode ready)")
            return serial_fd
        except Exception as e:
            serial_fd = None
    return None

def find_sd_mount():
    """Locate or auto-mount the SD card mount containing the MERO directory."""
    if DEFAULT_MOUNT.exists() and (DEFAULT_MOUNT / "MERO").exists():
        return DEFAULT_MOUNT

    base = Path("/run/media/zacmero")
    if base.exists():
        for p in base.iterdir():
            if p.is_dir() and (p / "MERO").exists():
                return p

    dev_path = run_cmd(f"blkid -U {DEV_UUID} 2>/dev/null")
    if not dev_path:
        raw = run_cmd("lsblk -lno NAME,UUID")
        for line in raw.splitlines():
            if DEV_UUID in line:
                dev_name = line.split()[0]
                dev_path = f"/dev/{dev_name}"
                break
    if not dev_path and os.path.exists("/dev/sde1"):
        dev_path = "/dev/sde1"

    if dev_path:
        run_cmd(f"udisksctl mount -b {dev_path} 2>/dev/null")
        if DEFAULT_MOUNT.exists() and (DEFAULT_MOUNT / "MERO").exists():
            return DEFAULT_MOUNT
        if base.exists():
            for p in base.iterdir():
                if p.is_dir() and (p / "MERO").exists():
                    return p

    return None

def find_target_player():
    """
    Locate the YouTube Music player instance on D-Bus.
    Priority 1: Player reporting music.youtube.com in xesam:url
    Priority 2: Player reporting music.youtube in metadata
    Priority 3: Any player currently playing
    Priority 4: First available MPRIS player
    """
    raw = run_cmd("playerctl -l 2>/dev/null")
    if not raw:
        return None
    players = [p.strip() for p in raw.splitlines() if p.strip()]
    if not players:
        return None

    for p in players:
        url = run_cmd(f'playerctl -p "{p}" metadata xesam:url 2>/dev/null')
        if "music.youtube.com" in url:
            return p

    for p in players:
        meta = run_cmd(f'playerctl -p "{p}" metadata --format "{{{{xesam:title}}}} {{{{xesam:album}}}} {{{{xesam:url}}}}" 2>/dev/null')
        if "music.youtube" in meta.lower():
            return p

    for p in players:
        status = run_cmd(f'playerctl -p "{p}" status 2>/dev/null')
        if status.lower() == "playing":
            return p

    return players[0]

def get_media_status(player):
    if not player:
        return None

    status = run_cmd(f'playerctl -p "{player}" status 2>/dev/null')
    if not status:
        return None

    fmt = "{{xesam:title}};;;{{xesam:artist}};;;{{xesam:album}};;;{{mpris:artUrl}};;;{{mpris:length}};;;{{position}};;;{{xesam:url}}"
    out = run_cmd(f'playerctl -p "{player}" metadata --format "{fmt}" 2>/dev/null')
    if not out or ";;;" not in out:
        return None

    parts = out.split(";;;")
    title = parts[0].strip() if len(parts) > 0 and parts[0].strip() else "Unknown Title"
    artist = parts[1].strip() if len(parts) > 1 and parts[1].strip() else "YouTube Music"
    album = parts[2].strip() if len(parts) > 2 and parts[2].strip() else "YouTube Music"
    art_url = parts[3].strip() if len(parts) > 3 else ""
    track_url = parts[6].strip() if len(parts) > 6 else ""

    length_sec = 0
    if len(parts) > 4 and parts[4].strip():
        try:
            length_sec = int(parts[4].strip()) // 1000000
        except ValueError:
            pass

    pos_sec = 0
    if len(parts) > 5 and parts[5].strip():
        try:
            pos_sec = int(float(parts[5].strip())) // 1000000
        except ValueError:
            pass
    if pos_sec == 0:
        pos_raw = run_cmd(f'playerctl -p "{player}" position 2>/dev/null')
        if pos_raw:
            try:
                pos_sec = int(float(pos_raw))
            except ValueError:
                pass

    return {
        "status": status.upper(),
        "title": title,
        "artist": artist,
        "album": album,
        "art_url": art_url,
        "track_url": track_url,
        "length": length_sec,
        "position": pos_sec
    }

def extract_video_id(track_url):
    """Extract YouTube video ID from xesam:url (music.youtube.com or youtube.com)."""
    if not track_url:
        return ""
    # ?v=ID or /v/ID
    for sep in ("?v=", "&v=", "/v/", "/vi/"):
        if sep in track_url:
            vid = track_url.split(sep, 1)[1]
            vid = vid.split("&")[0].split("?")[0].split("/")[0]
            if vid:
                return vid
    return ""

def update_artwork(art_url, sd_mount, track_url=""):
    global last_art_url
    if not sd_mount:
        return

    # Prefer YouTube CDN thumbnail keyed on video ID (high-res, per-track unique)
    video_id = extract_video_id(track_url)
    if video_id:
        cache_key = video_id
        cdn_url = f"https://i.ytimg.com/vi/{video_id}/maxresdefault.jpg"
        fallback_url = f"https://i.ytimg.com/vi/{video_id}/hqdefault.jpg"
    else:
        cache_key = art_url
        cdn_url = art_url
        fallback_url = ""

    if not cache_key or cache_key == last_art_url:
        return

    tmp_raw = Path("/tmp/mero_cover_raw")
    tmp_out = Path("/tmp/mero_cover.jpg")

    try:
        fetched = False
        for url in filter(None, [cdn_url, fallback_url, art_url if art_url.startswith("http") else ""]):
            if not url:
                continue
            try:
                req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = resp.read()
                    if len(data) > 500:  # sanity: reject empty/error pages
                        tmp_raw.write_bytes(data)
                        fetched = True
                        break
            except Exception:
                continue

        # Fallback: local MPRIS file
        if not fetched and art_url.startswith("file://"):
            local_path = art_url[7:]
            if os.path.exists(local_path):
                shutil.copyfile(local_path, tmp_raw)
                fetched = True

        if fetched and tmp_raw.exists():
            subprocess.run(
                f'ffmpeg -y -i "{tmp_raw}" -vf "scale=180:180:force_original_aspect_ratio=increase,crop=180:180" -q:v 2 "{tmp_out}" -loglevel quiet',
                shell=True, timeout=5
            )
            if tmp_out.exists():
                stream_dir = sd_mount / "Stream"
                mero_dir = sd_mount / "MERO"
                stream_dir.mkdir(parents=True, exist_ok=True)
                mero_dir.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(tmp_out, stream_dir / "cover.jpg")
                shutil.copyfile(tmp_out, mero_dir / "cover.jpg")
                last_art_url = cache_key
                src = "YT CDN" if video_id else "MPRIS"
                print(f"[+] Cover art updated ({src}, vid={video_id or 'n/a'}) → 180x180 JPEG")
    except Exception as e:
        print(f"[!] Artwork fetch error: {e}")

def execute_transport_command(cmd, player):
    """Execute media control command against targeted player."""
    p_flag = f'-p "{player}" ' if player else ""
    print(f"[+] EXECUTING COMMAND: '{cmd}' -> Player: {player}")
    if cmd == "PLAY_PAUSE":
        run_cmd(f"playerctl {p_flag}play-pause")
    elif cmd == "NEXT":
        run_cmd(f"playerctl {p_flag}next")
    elif cmd == "PREV":
        run_cmd(f"playerctl {p_flag}previous")
    elif cmd == "VOL_UP":
        run_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+")
    elif cmd == "VOL_DOWN":
        run_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-")

def check_serial_commands(player):
    """Process incoming packets over /dev/ttyUSB0."""
    global rx_serial_buf
    fd = check_serial_connection()
    if fd is None:
        return

    try:
        r, _, _ = select.select([fd], [], [], 0.02)
        if r:
            chunk = os.read(fd, 256).decode("ascii", errors="ignore")
            rx_serial_buf += chunk
            while "\n" in rx_serial_buf:
                line, rx_serial_buf = rx_serial_buf.split("\n", 1)
                line = line.strip()
                if not line:
                    continue
                print(f"[+] Serial Line from GPS: '{line}'")
                if "PING" in line or "SYNC" in line:
                    try:
                        os.write(fd, b"MERO:ACK\n")
                    except Exception:
                        pass
                elif "CMD:" in line:
                    # e.g. MERO:CMD:PLAY_PAUSE:1
                    parts = line.split(":")
                    cmd = parts[2] if len(parts) >= 3 else parts[1]
                    execute_transport_command(cmd, player)
                elif line in ["PLAY_PAUSE", "NEXT", "PREV", "VOL_UP", "VOL_DOWN"]:
                    execute_transport_command(line, player)
    except Exception as e:
        print(f"[!] Serial read error: {e}")
        try:
            os.close(fd)
        except Exception:
            pass
        serial_fd = None

def send_serial_track_update(media):
    """Stream live track metadata packet to WinCE over /dev/ttyUSB0."""
    global last_serial_send_time, serial_fd
    fd = check_serial_connection()
    if fd is None or not media:
        return

    now = time.time()
    if now - last_serial_send_time < 0.35:
        return
    last_serial_send_time = now

    msg = f"MERO:NOW:status={media['status']}|title={media['title']}|artist={media['artist']}|album={media['album']}|pos={media['position']}|len={media['length']}\n"
    try:
        os.write(fd, msg.encode("utf-8"))
    except Exception as e:
        try:
            os.close(fd)
        except Exception:
            pass
        serial_fd = None

def check_storage_commands(sd_mount, player):
    """Check for commands written by WinCE to SD card mailbox."""
    global last_processed_seq, last_processed_cmd
    if not sd_mount:
        return
    cmd_files = [
        sd_mount / "MERO" / "media_cmd.txt",
        sd_mount / "Stream" / "media_cmd.txt"
    ]

    for cmd_file in cmd_files:
        if not cmd_file.exists():
            continue
        try:
            raw = cmd_file.read_text(encoding="utf-8", errors="ignore").strip()
            if not raw or "PROCESSED" in raw or "IDLE" in raw:
                continue

            cmd = ""
            seq = None

            for line in raw.splitlines():
                line = line.strip()
                if line.startswith("seq="):
                    try:
                        seq = int(line.split("=", 1)[1])
                    except ValueError:
                        pass
                elif line.startswith("cmd="):
                    cmd = line.split("=", 1)[1].strip()

            if not cmd and (raw.isalnum() or "_" in raw):
                cmd = raw

            if not cmd:
                continue

            is_new = False
            if seq is not None:
                if seq != last_processed_seq:
                    is_new = True
                    last_processed_seq = seq
            else:
                if cmd != last_processed_cmd:
                    is_new = True
                    last_processed_cmd = cmd

            if is_new:
                execute_transport_command(cmd, player)

                ack_payload = f"seq={seq or 0}\ncmd=PROCESSED\n".encode("utf-8")
                try:
                    fd = os.open(str(cmd_file), os.O_WRONLY | os.O_SYNC)
                    os.lseek(fd, 0, os.SEEK_SET)
                    os.write(fd, ack_payload)
                    os.ftruncate(fd, len(ack_payload))
                    os.close(fd)
                except Exception:
                    pass

        except Exception as e:
            print(f"[!] Error processing command file {cmd_file}: {e}")

def write_now_playing(sd_mount, media):
    """Write now_playing.txt synchronously without changing file clusters."""
    if not sd_mount or not media:
        return

    stream_dir = sd_mount / "Stream"
    mero_dir = sd_mount / "MERO"
    stream_dir.mkdir(parents=True, exist_ok=True)
    mero_dir.mkdir(parents=True, exist_ok=True)

    content = (
        f"title={media['title']}\r\n"
        f"artist={media['artist']}\r\n"
        f"album={media['album']}\r\n"
        f"status={media['status']}\r\n"
        f"position={media['position']}\r\n"
        f"length={media['length']}\r\n"
        f"cover=\\SDMMC\\Stream\\cover.jpg\r\n"
    ).encode("utf-8")

    for target in [stream_dir / "now_playing.txt", mero_dir / "now_playing.txt"]:
        try:
            fd = os.open(str(target), os.O_WRONLY | os.O_CREAT | os.O_SYNC, 0o666)
            os.lseek(fd, 0, os.SEEK_SET)
            os.write(fd, content)
            os.ftruncate(fd, len(content))
            os.close(fd)
        except Exception:
            try:
                target.write_bytes(content)
            except Exception:
                pass

def main():
    global active_player, last_title, last_status
    sys.stdout.reconfigure(line_buffering=True)
    print("=== MERO MONITOR #2: DUAL-STACK MEDIA BRIDGE ===")
    print("[*] Initializing USB Serial (/dev/ttyUSB0) & Storage Mailbox...")

    poll_count = 0
    while True:
        try:
            # 1. Maintain active player lock
            if poll_count % 5 == 0 or not active_player:
                found_player = find_target_player()
                if found_player != active_player:
                    active_player = found_player
                    print(f"[+] Locked onto media player: {active_player}")

            # 2. Check incoming commands from Serial (ActiveSync mode)
            check_serial_commands(active_player)

            # 3. Check incoming commands from Storage (Mass Storage mode)
            sd_mount = find_sd_mount()
            if sd_mount:
                check_storage_commands(sd_mount, active_player)

            # 4. Query live player status
            media = get_media_status(active_player)
            if media:
                if media["title"] != last_title or media["status"] != last_status:
                    print(f"[*] Track: {media['artist']} - {media['title']} [{media['status']}] ({media['position']}s / {media['length']}s)")
                    last_title = media["title"]
                    last_status = media["status"]

                # Send real-time updates over Serial Link
                send_serial_track_update(media)

                # Send updates to SD card if storage is available
                if sd_mount:
                    update_artwork(media["art_url"], sd_mount, media.get("track_url", ""))
                    write_now_playing(sd_mount, media)

            poll_count += 1
            time.sleep(0.15)
        except KeyboardInterrupt:
            break
        except Exception as e:
            time.sleep(0.3)

    if serial_fd:
        try:
            os.close(serial_fd)
        except Exception:
            pass

if __name__ == "__main__":
    main()
