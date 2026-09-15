#!/usr/bin/env python3
"""
mero_media_bridge.py - Host Bridge for Mero Monitor #2 YouTube Music Deck
Connects host media players (MPRIS / playerctl) to Windows CE companion deck.
Synchronizes track metadata, album artwork, and bidirectional transport controls.
"""

import os
import sys
import time
import shutil
import urllib.request
import subprocess
from pathlib import Path

SD_MOUNT = Path("/run/media/zacmero/6232-3562")
STREAM_DIR = SD_MOUNT / "Stream"
MERO_DIR = SD_MOUNT / "MERO"
SERIAL_PORT = "/dev/ttyUSB0"

last_art_url = ""
last_title = ""
last_status = ""

def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=2)
        return res.stdout.strip()
    except Exception:
        return ""

def get_media_status():
    status = run_cmd("playerctl status 2>/dev/null")
    if not status:
        return None

    # Query metadata with delimiter
    fmt = "{{xesam:title}};;;{{xesam:artist}};;;{{xesam:album}};;;{{mpris:artUrl}};;;{{mpris:length}};;;{{position}}"
    out = run_cmd(f'playerctl metadata --format "{fmt}" 2>/dev/null')
    if not out or ";;;" not in out:
        return None

    parts = out.split(";;;")
    title = parts[0].strip() if len(parts) > 0 else "Unknown Title"
    artist = parts[1].strip() if len(parts) > 1 else "Unknown Artist"
    album = parts[2].strip() if len(parts) > 2 else "YouTube Music"
    art_url = parts[3].strip() if len(parts) > 3 else ""

    length_us = 0
    try:
        length_us = int(parts[4].strip()) if len(parts) > 4 and parts[4].strip() else 0
    except ValueError:
        pass
    length_sec = length_us // 1000000

    pos_us = 0
    try:
        pos_us = int(float(parts[5].strip())) if len(parts) > 5 and parts[5].strip() else 0
    except ValueError:
        pass
    pos_sec = pos_us // 1000000

    return {
        "status": status.upper(),
        "title": title,
        "artist": artist,
        "album": album,
        "art_url": art_url,
        "length": length_sec,
        "position": pos_sec
    }

def update_artwork(art_url):
    global last_art_url
    if not art_url or art_url == last_art_url:
        return

    tmp_raw = Path("/tmp/mero_cover_raw")
    tmp_out = Path("/tmp/mero_cover.jpg")

    try:
        if art_url.startswith("file://"):
            local_path = art_url[7:]
            if os.path.exists(local_path):
                shutil.copyfile(local_path, tmp_raw)
        elif art_url.startswith("http://") or art_url.startswith("https://"):
            req = urllib.request.Request(art_url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=5) as resp, open(tmp_raw, "wb") as f:
                f.write(resp.read())

        # Resize to 180x180 JPEG using PIL or ffmpeg
        if tmp_raw.exists():
            subprocess.run(
                f'ffmpeg -y -i "{tmp_raw}" -vf "scale=180:180:force_original_aspect_ratio=increase,crop=180:180" -q:v 3 "{tmp_out}" -loglevel quiet',
                shell=True, timeout=3
            )
            if tmp_out.exists() and SD_MOUNT.exists():
                STREAM_DIR.mkdir(parents=True, exist_ok=True)
                MERO_DIR.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(tmp_out, STREAM_DIR / "cover.jpg")
                shutil.copyfile(tmp_out, MERO_DIR / "cover.jpg")
                last_art_url = art_url
    except Exception as e:
        print(f"[!] Artwork fetch error: {e}")

def check_commands():
    cmd_file = MERO_DIR / "media_cmd.txt"
    if cmd_file.exists():
        try:
            cmd = cmd_file.read_text().strip()
            cmd_file.unlink()
            print(f"[+] Received command from GPS: {cmd}")
            if cmd == "PLAY_PAUSE":
                run_cmd("playerctl play-pause")
            elif cmd == "NEXT":
                run_cmd("playerctl next")
            elif cmd == "PREV":
                run_cmd("playerctl previous")
            elif cmd == "VOL_UP":
                run_cmd("playerctl volume 0.05+")
            elif cmd == "VOL_DOWN":
                run_cmd("playerctl volume 0.05-")
        except Exception as e:
            print(f"[!] Error processing command file: {e}")

def check_serial_commands(fd):
    if fd is None:
        return
    try:
        import select
        r, _, _ = select.select([fd], [], [], 0.01)
        if r:
            data = os.read(fd, 256).decode("ascii", errors="ignore").strip()
            if data:
                print(f"[+] Serial Command: {data}")
                if "PLAY" in data:
                    run_cmd("playerctl play-pause")
                elif "NEXT" in data:
                    run_cmd("playerctl next")
                elif "PREV" in data:
                    run_cmd("playerctl previous")
    except Exception:
        pass

def main():
    print("=== MERO MONITOR #2: YOUTUBE MUSIC HOST BRIDGE ===")
    print("[*] Monitoring MPRIS media players (YouTube Music / Chrome / Firefox)...")

    # Try opening serial port non-blocking
    serial_fd = None
    if os.path.exists(SERIAL_PORT):
        try:
            serial_fd = os.open(SERIAL_PORT, os.O_RDWR | os.O_NONBLOCK)
            print(f"[+] Attached to serial link at {SERIAL_PORT}")
        except Exception:
            pass

    while True:
        try:
            # 1. Process incoming commands
            check_commands()
            check_serial_commands(serial_fd)

            # 2. Query host media status
            media = get_media_status()
            if media and SD_MOUNT.exists():
                STREAM_DIR.mkdir(parents=True, exist_ok=True)
                MERO_DIR.mkdir(parents=True, exist_ok=True)

                update_artwork(media["art_url"])

                content = (
                    f"title={media['title']}\r\n"
                    f"artist={media['artist']}\r\n"
                    f"album={media['album']}\r\n"
                    f"status={media['status']}\r\n"
                    f"position={media['position']}\r\n"
                    f"length={media['length']}\r\n"
                    f"cover=\\SDMMC\\Stream\\cover.jpg\r\n"
                )

                np_stream = STREAM_DIR / "now_playing.txt"
                np_mero = MERO_DIR / "now_playing.txt"

                np_stream.write_text(content, encoding="utf-8")
                np_mero.write_text(content, encoding="utf-8")

            time.sleep(0.8)
        except KeyboardInterrupt:
            break
        except Exception as e:
            time.sleep(1)

    if serial_fd:
        os.close(serial_fd)

if __name__ == "__main__":
    main()
