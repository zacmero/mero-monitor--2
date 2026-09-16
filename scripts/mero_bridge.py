#!/usr/bin/env python3
"""
mero_bridge.py - Unified Host Bridge for Mero Companion Deck
Project: mero-monitor-#2
Features:
  1. YouTube Music / MPRIS: Real-time metadata, title, artist, album, progress, duration, cover art
  2. Cyberpunk Meteorological Telemetry: Live weather, humidity, wind, pressure, UV, 3-day forecast (wttr.in)
  3. Dual-Transport: Direct in-place SDMMC FAT32 storage sync + optional ActiveSync serial stream (/dev/ttyUSB0)
"""

import os
import sys
import time
import json
import shutil
import datetime
import urllib.request
import subprocess
from pathlib import Path

DEFAULT_MOUNT = Path("/run/media/zacmero/6232-3562")
DEV_UUID = "6232-3562"
SERIAL_PORT = "/dev/ttyUSB0"

# Media state
last_art_url = ""
last_title = ""
last_status = ""
active_player = ""
_last_now_playing_content = b""

# Weather state
last_weather_fetch = 0.0
WEATHER_INTERVAL = 600.0  # 10 minutes
cached_weather_content = ""

# Serial connection state
serial_fd = None
last_serial_send_time = 0.0


def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=2)
        return res.stdout.strip()
    except Exception:
        return ""


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
    if dev_path:
        run_cmd(f"udisksctl mount -b {dev_path} 2>/dev/null")
        if DEFAULT_MOUNT.exists() and (DEFAULT_MOUNT / "MERO").exists():
            return DEFAULT_MOUNT
        if base.exists():
            for p in base.iterdir():
                if p.is_dir() and (p / "MERO").exists():
                    return p
    return None


# ====================================================================
# SERIAL PORT STREAMING (ActiveSync mode)
# ====================================================================
def init_serial():
    global serial_fd
    if not os.path.exists(SERIAL_PORT):
        if serial_fd is not None:
            try:
                os.close(serial_fd)
            except Exception:
                pass
            serial_fd = None
        return None

    if serial_fd is not None:
        return serial_fd

    try:
        import termios

        fd = os.open(SERIAL_PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(fd)
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        serial_fd = fd
        print(f"[+] Connected to ActiveSync serial port: {SERIAL_PORT}")
        return serial_fd
    except Exception as e:
        serial_fd = None
        return None


def send_serial(line):
    global serial_fd
    fd = init_serial()
    if fd is None:
        return
    try:
        data = (line.strip() + "\n").encode("utf-8")
        os.write(fd, data)
    except Exception:
        try:
            os.close(fd)
        except Exception:
            pass
        serial_fd = None


# ====================================================================
# WEATHER TELEMETRY (wttr.in)
# ====================================================================
def fetch_weather(sd_mount=None):
    global last_weather_fetch, cached_weather_content
    now = time.time()
    if cached_weather_content and (now - last_weather_fetch) < WEATHER_INTERVAL:
        return cached_weather_content

    try:
        req = urllib.request.Request(
            "https://wttr.in/?format=j1",
            headers={"User-Agent": "curl/7.88.1 (Mero-Companion-Deck)"}
        )
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        cc = data["current_condition"][0]
        area = data["nearest_area"][0]
        loc = f"{area['areaName'][0]['value']}, {area['region'][0]['value']} [BR]"
        w = data["weather"]

        content = (
            f"location={loc}\n"
            f"coord=LAT -29.36 | LON -50.81\n"
            f"desc={cc['weatherDesc'][0]['value']}\n"
            f"temp_c={cc['temp_C']}\n"
            f"feels_like={cc['FeelsLikeC']}\n"
            f"humidity={cc['humidity']}\n"
            f"wind={cc['windspeedKmph']} km/h {cc['winddir16Point']}\n"
            f"pressure={cc['pressure']} hPa\n"
            f"precip={cc['precipMM']} mm\n"
            f"uv={cc['uvIndex']}\n"
            f"updated={cc['observation_time']}\n"
            f"d1_name=TODAY\n"
            f"d1_desc={w[0]['hourly'][4]['weatherDesc'][0]['value']}\n"
            f"d1_min={w[0]['mintempC']}\n"
            f"d1_max={w[0]['maxtempC']}\n"
            f"d2_name=TOMORROW\n"
            f"d2_desc={w[1]['hourly'][4]['weatherDesc'][0]['value']}\n"
            f"d2_min={w[1]['mintempC']}\n"
            f"d2_max={w[1]['maxtempC']}\n"
            f"d3_name=DAY+2\n"
            f"d3_desc={w[2]['hourly'][4]['weatherDesc'][0]['value']}\n"
            f"d3_min={w[2]['mintempC']}\n"
            f"d3_max={w[2]['maxtempC']}\n"
        )

        cached_weather_content = content
        last_weather_fetch = now

        # Write to SD card in-place
        if sd_mount:
            dest = sd_mount / "MERO" / "weather.txt"
            try:
                dest.parent.mkdir(parents=True, exist_ok=True)
                with open(dest, "w", encoding="utf-8") as f:
                    f.write(content)
                    f.flush()
                os.sync()
            except Exception as e:
                print(f"[!] Weather write error: {e}")

        # Broadcast over serial
        wx_line = (
            f"MERO:WX:temp={cc['temp_C']}|feels={cc['FeelsLikeC']}|hum={cc['humidity']}|"
            f"cond={cc['weatherDesc'][0]['value']}|wind={cc['windspeedKmph']} km/h {cc['winddir16Point']}|"
            f"press={cc['pressure']} hPa|precip={cc['precipMM']} mm|uv={cc['uvIndex']}|up={cc['observation_time']}"
        )
        send_serial(wx_line)
        print(f"[+] Weather updated: {loc} -> {cc['temp_C']}°C ({cc['weatherDesc'][0]['value']})")
        return content

    except Exception as e:
        print(f"[!] Weather fetch error: {e}")
        return cached_weather_content


# ====================================================================
# MEDIA CONTROLLER (YouTube Music / MPRIS)
# ====================================================================
def find_target_player():
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

    fmt = "{{xesam:title}};;;{{xesam:artist}};;;{{xesam:album}};;;{{mpris:artUrl}};;;{{mpris:length}};;;{{xesam:url}}"
    out = run_cmd(f'playerctl -p "{player}" metadata --format "{fmt}" 2>/dev/null')
    if not out or ";;;" not in out:
        return None

    parts = out.split(";;;")
    title = parts[0].strip() if len(parts) > 0 and parts[0].strip() else "Unknown Title"
    artist = parts[1].strip() if len(parts) > 1 and parts[1].strip() else "YouTube Music"
    album = parts[2].strip() if len(parts) > 2 and parts[2].strip() else "YouTube Music"
    art_url = parts[3].strip() if len(parts) > 3 else ""
    track_url = parts[5].strip() if len(parts) > 5 else ""

    length_sec = 0
    if len(parts) > 4 and parts[4].strip():
        try:
            length_sec = int(parts[4].strip()) // 1000000
        except ValueError:
            pass

    pos_sec = 0
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
    if not track_url:
        return ""
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

    dest_mero = sd_mount / "MERO" / "cover.jpg"
    dest_mero.parent.mkdir(parents=True, exist_ok=True)

    fetched = False
    if cdn_url.startswith("http"):
        try:
            req = urllib.request.Request(cdn_url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=5) as resp:
                data = resp.read()
                if len(data) > 2000:
                    with open(dest_mero, "wb") as f:
                        f.write(data)
                        f.flush()
                    os.sync()
                    last_art_url = cache_key
                    print(f"[+] Downloaded YouTube HD cover art: {len(data)} bytes")
                    fetched = True
        except Exception:
            pass

        if not fetched and fallback_url:
            try:
                req = urllib.request.Request(fallback_url, headers={"User-Agent": "Mozilla/5.0"})
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = resp.read()
                    if len(data) > 1000:
                        with open(dest_mero, "wb") as f:
                            f.write(data)
                            f.flush()
                        os.sync()
                        last_art_url = cache_key
                        print(f"[+] Downloaded YouTube HQ cover art: {len(data)} bytes")
                        fetched = True
            except Exception:
                pass

    elif cdn_url.startswith("file://"):
        local_path = Path(cdn_url.replace("file://", ""))
        if local_path.exists():
            try:
                shutil.copyfile(local_path, dest_mero)
                os.sync()
                last_art_url = cache_key
                print(f"[+] Copied local cover art: {local_path.name}")
            except Exception:
                pass


def write_now_playing(sd_mount, media):
    global _last_now_playing_content, last_serial_send_time

    content = (
        f"title={media['title']}\r\n"
        f"artist={media['artist']}\r\n"
        f"album={media['album']}\r\n"
        f"status={media['status']}\r\n"
        f"position={media['position']}\r\n"
        f"length={media['length']}\r\n"
        f"cover=\\SDMMC\\MERO\\cover.jpg\r\n"
    ).encode("utf-8")

    now = time.time()
    # Write to SD card in-place if changed or periodically
    if content != _last_now_playing_content:
        dest_mero = sd_mount / "MERO" / "now_playing.txt"
        try:
            dest_mero.parent.mkdir(parents=True, exist_ok=True)
            with open(dest_mero, "wb") as f:
                f.write(content)
                f.flush()
            os.sync()
            _last_now_playing_content = content
        except Exception as e:
            print(f"[!] Storage write error: {e}")

    # Serial streaming (max 2 times per second)
    if (now - last_serial_send_time) >= 0.5:
        line = (
            f"MERO:NOW:status={media['status']}|title={media['title']}|"
            f"artist={media['artist']}|album={media['album']}|"
            f"pos={media['position']}|len={media['length']}"
        )
        send_serial(line)
        last_serial_send_time = now


# ====================================================================
# MAIN BRIDGE LOOP
# ====================================================================
def main():
    global active_player, last_title, last_status, last_art_url
    sys.stdout.reconfigure(line_buffering=True)
    print("=== MERO COMPANION DECK: UNIFIED HOST DAEMON ===")
    print("  [+] YouTube Music MPRIS View")
    print("  [+] Cyberpunk Meteorological Telemetry (Canela, RS)")

    sd_mount = find_sd_mount()
    if sd_mount:
        print(f"[+] SD card mounted at: {sd_mount}")
        fetch_weather(sd_mount)

    poll_count = 0
    while True:
        try:
            # 1. Mount maintenance
            if poll_count % 30 == 0 or not sd_mount:
                new_mount = find_sd_mount()
                if new_mount != sd_mount:
                    sd_mount = new_mount
                    if sd_mount:
                        print(f"[+] SD card detected at: {sd_mount}")
                        fetch_weather(sd_mount)

            # 2. Weather telemetry update
            if poll_count % 50 == 0:
                fetch_weather(sd_mount)

            # 3. Media player tracking
            if poll_count % 10 == 0 or not active_player:
                found_player = find_target_player()
                if found_player != active_player:
                    active_player = found_player
                    if active_player:
                        print(f"[+] Locked onto player: {active_player}")

            media = get_media_status(active_player)
            if media and sd_mount:
                if media["title"] != last_title or media["status"] != last_status:
                    if media["title"] != last_title:
                        last_art_url = ""
                    last_title = media["title"]
                    last_status = media["status"]
                    print(f"[*] Track: {media['artist']} - {media['title']} [{media['status']}] ({media['position']}s / {media['length']}s)")

                update_artwork(media["art_url"], sd_mount, media.get("track_url", ""))
                write_now_playing(sd_mount, media)

            poll_count += 1
            time.sleep(0.5)

        except KeyboardInterrupt:
            print("\n[+] Daemon stopped cleanly.")
            break
        except Exception as e:
            time.sleep(1.0)


if __name__ == "__main__":
    main()
