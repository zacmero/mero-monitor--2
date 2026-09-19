#!/usr/bin/env python3
"""
mero_bridge.py - Unified Host Bridge for Mero Companion Deck
Project: mero-monitor-#2
Features:
  1. YouTube Music / MPRIS: Strictly binds to music.youtube.com, real-time metadata,
     accurate position, duration, and HD album art extraction (never hijacked by normal YouTube tabs).
  2. Cyberpunk Meteorological Telemetry: Live weather, humidity, wind, pressure, UV, 3-day forecast (wttr.in).
  3. ActiveSync PPP/TCP streaming, with SDMMC used only for deployment.
"""

import os
import sys
import time
import json
import shutil
import io
import urllib.request
import urllib.parse
import subprocess
import socket
from pathlib import Path
from PIL import Image

DEFAULT_MOUNT = Path("/run/media/zacmero/6232-3562")
DEV_UUID = "6232-3562"
STREAM_HOST = "192.168.55.101"
STREAM_PORT = 5000

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

# PPP/TCP stream state
stream_listener = None
stream_client = None
last_stream_send_time = 0.0
latest_weather_line = ""


def run_cmd(cmd):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=2)
        return res.stdout.strip()
    except Exception:
        return ""


def find_sd_mount():
    """Locate or auto-mount the SD card mount containing the MERO directory."""
    if os.path.ismount(DEFAULT_MOUNT) and (DEFAULT_MOUNT / "MERO").exists():
        return DEFAULT_MOUNT

    base = Path("/run/media/zacmero")
    if base.exists():
        for p in base.iterdir():
            if os.path.ismount(p) and (p / "MERO").exists():
                return p

    dev_path = run_cmd(f"blkid -U {DEV_UUID} 2>/dev/null")
    if dev_path:
        run_cmd(f"udisksctl mount -b {dev_path} 2>/dev/null")
        if os.path.ismount(DEFAULT_MOUNT) and (DEFAULT_MOUNT / "MERO").exists():
            return DEFAULT_MOUNT
        if base.exists():
            for p in base.iterdir():
                if os.path.ismount(p) and (p / "MERO").exists():
                    return p
    return None


LOCAL_MEDIA_EXE = Path("/home/zacmero/projects/mero-monitor-#2/wince/media/mero-media-ctrl.exe")
_last_synced_mtime = 0.0

def sync_binaries(sd_mount):
    global _last_synced_mtime
    if not sd_mount or not LOCAL_MEDIA_EXE.exists():
        return
    try:
        local_mtime = LOCAL_MEDIA_EXE.stat().st_mtime
        if local_mtime == _last_synced_mtime:
            return
        dest = sd_mount / "MERO" / "mero-media-ctrl.exe"
        if not dest.exists() or (local_mtime - dest.stat().st_mtime > 3.0) or (LOCAL_MEDIA_EXE.stat().st_size != dest.stat().st_size):
            shutil.copy2(LOCAL_MEDIA_EXE, dest)
            os.sync()
            _last_synced_mtime = local_mtime
            print(f"[+] Auto-deployed updated mero-media-ctrl.exe ({LOCAL_MEDIA_EXE.stat().st_size}B) to {dest}")
        else:
            _last_synced_mtime = local_mtime
    except Exception as e:
        print(f"[!] Auto-deploy error: {e}")



# ====================================================================
# TCP STREAMING OVER ACTIVESYNC PPP
# ====================================================================
def init_stream_server():
    global stream_listener
    if stream_listener is not None:
        return
    try:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind((STREAM_HOST, STREAM_PORT))
        listener.listen(1)
        listener.setblocking(False)
        stream_listener = listener
        print(f"[+] Media stream listening on {STREAM_HOST}:{STREAM_PORT}")
    except OSError:
        if 'listener' in locals():
            listener.close()


def send_stream(line):
    global stream_client
    init_stream_server()
    if stream_listener is None:
        return
    if stream_client is None:
        try:
            stream_client, address = stream_listener.accept()
            stream_client.settimeout(0.2)
            print(f"[+] WinCE media client connected from {address[0]}")
            if latest_weather_line:
                stream_client.sendall((latest_weather_line + "\n").encode("utf-8"))
        except BlockingIOError:
            return
    try:
        stream_client.sendall((line.strip() + "\n").encode("utf-8"))
    except OSError:
        stream_client.close()
        stream_client = None


# ====================================================================
# WEATHER TELEMETRY (wttr.in)
# ====================================================================
def get_configured_city(sd_mount=None):
    if os.environ.get("MERO_WEATHER_CITY"):
        return os.environ.get("MERO_WEATHER_CITY").strip()
    user_conf = Path.home() / ".config" / "mero" / "weather.conf"
    if user_conf.exists():
        try:
            for line in user_conf.read_text().splitlines():
                if line.startswith("city="):
                    return line.split("=", 1)[1].strip()
        except Exception:
            pass
    if sd_mount:
        sd_conf = sd_mount / "MERO" / "weather.conf"
        if sd_conf.exists():
            try:
                for line in sd_conf.read_text().splitlines():
                    if line.startswith("city="):
                        return line.split("=", 1)[1].strip()
            except Exception:
                pass
    return ""


def fetch_weather(sd_mount=None):
    global last_weather_fetch, cached_weather_content, latest_weather_line
    now = time.time()
    if cached_weather_content and (now - last_weather_fetch) < WEATHER_INTERVAL:
        if sd_mount:
            dest = sd_mount / "MERO" / "weather.txt"
            try:
                if not dest.exists() or dest.read_text(encoding="utf-8", errors="ignore") != cached_weather_content:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_text(cached_weather_content, encoding="utf-8")
                    os.sync()
            except Exception:
                pass
        return cached_weather_content

    try:
        city = get_configured_city(sd_mount)
        city_param = urllib.parse.quote(city) if city else ""
        req = urllib.request.Request(
            f"https://wttr.in/{city_param}?format=j1",
            headers={"User-Agent": "curl/7.88.1 (Mero-Companion-Deck)"}
        )
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read().decode("utf-8"))

        cc = data["current_condition"][0]
        area = data["nearest_area"][0]
        area_name = area['areaName'][0]['value'] if 'areaName' in area and area['areaName'] else (city or "LOCAL")
        region_name = area['region'][0]['value'] if 'region' in area and area['region'] else "BR"
        if "RIO GRANDE DO SUL" in region_name.upper():
            region_name = "RS"
        loc = f"{area_name.upper()}, {region_name.upper()} [BR]"
        lat = area.get('latitude', '-29.36')
        lon = area.get('longitude', '-50.81')
        w = data["weather"]
        now_str = time.strftime("%Y-%m-%d %H:%M:%S")

        content = (
            f"location={loc}\n"
            f"coord=LAT {lat} | LON {lon}\n"
            f"desc={cc['weatherDesc'][0]['value']}\n"
            f"temp_c={cc['temp_C']}\n"
            f"feels_like={cc['FeelsLikeC']}\n"
            f"humidity={cc['humidity']}\n"
            f"wind={cc['windspeedKmph']} km/h {cc['winddir16Point']}\n"
            f"pressure={cc['pressure']} hPa\n"
            f"precip={cc['precipMM']} mm\n"
            f"uv={cc['uvIndex']}\n"
            f"updated={cc['observation_time']}\n"
            f"host_time={now_str}\n"
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

        # Broadcast over ActiveSync PPP/TCP.
        wx_line = (
            f"MERO:WX:temp={cc['temp_C']}|feels={cc['FeelsLikeC']}|hum={cc['humidity']}|"
            f"cond={cc['weatherDesc'][0]['value']}|wind={cc['windspeedKmph']} km/h {cc['winddir16Point']}|"
            f"press={cc['pressure']} hPa|precip={cc['precipMM']} mm|uv={cc['uvIndex']}|up={cc['observation_time']}|time={now_str}|loc={loc}"
        )
        latest_weather_line = wx_line
        send_stream(wx_line)
        print(f"[+] Weather updated: {loc} -> {cc['temp_C']}°C ({cc['weatherDesc'][0]['value']})")
        return content

    except Exception as e:
        print(f"[!] Weather fetch error: {e}")
        return cached_weather_content


# ====================================================================
# STRICT YOUTUBE MUSIC TARGETING (Never hijacked by video tabs)
# ====================================================================
def find_target_player():
    raw = run_cmd("playerctl -l 2>/dev/null")
    if not raw:
        return None
    players = [p.strip() for p in raw.splitlines() if p.strip()]
    if not players:
        return None

    # Priority 1: Explicit music.youtube.com URL (Playing or Paused)
    for p in players:
        url = run_cmd(f'playerctl -p "{p}" metadata xesam:url 2>/dev/null')
        if "music.youtube.com" in url:
            return p

    # Priority 2: Player metadata containing 'music.youtube' or 'YouTube Music'
    for p in players:
        meta = run_cmd(f'playerctl -p "{p}" metadata --format "{{{{xesam:title}}}} {{{{xesam:album}}}} {{{{xesam:artist}}}} {{{{xesam:url}}}}" 2>/dev/null')
        meta_low = meta.lower()
        if "music.youtube" in meta_low or "youtube music" in meta_low:
            return p

    # Priority 3: Native desktop music players ONLY (Spotify, MPD, Audacious, etc.)
    # STRICT FILTER: Never match browser tabs or standard youtube video tabs!
    for p in players:
        url = run_cmd(f'playerctl -p "{p}" metadata xesam:url 2>/dev/null')
        if "youtube.com" in url or any(b in p.lower() for b in ("firefox", "chrom", "brave", "edge", "opera")):
            continue
        status = run_cmd(f'playerctl -p "{p}" status 2>/dev/null')
        if status.lower() == "playing":
            return p

    return None


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
    title = parts[0].strip() if len(parts) > 0 and parts[0].strip() else "Unknown Track"
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


def process_and_save_cover(raw_bytes, dest_path):
    try:
        img = Image.open(io.BytesIO(raw_bytes)).convert("RGB")
        w, h = img.size
        if w > h:
            offset = (w - h) // 2
            img = img.crop((offset, 0, offset + h, h))
        elif h > w:
            offset = (h - w) // 2
            img = img.crop((0, offset, w, offset + w))
        img = img.resize((214, 214), Image.LANCZOS)
        img.save(dest_path, "JPEG", quality=90)
        os.sync()
        return True
    except Exception as e:
        print(f"[!] Cover processing error: {e}")
        try:
            with open(dest_path, "wb") as f:
                f.write(raw_bytes)
                f.flush()
            os.sync()
            return True
        except Exception:
            return False


def update_artwork(art_url, sd_mount, track_url="", title=""):
    global last_art_url
    if not sd_mount:
        return

    video_id = extract_video_id(track_url)
    cache_key = video_id if video_id else (art_url if art_url else title)

    if not cache_key or cache_key == last_art_url:
        return

    dest_mero = sd_mount / "MERO" / "cover.jpg"
    dest_mero.parent.mkdir(parents=True, exist_ok=True)

    # 1. If video_id is present, prioritize YouTube CDN HD cover art
    if video_id:
        cdn_urls = [
            f"https://i.ytimg.com/vi/{video_id}/maxresdefault.jpg",
            f"https://i.ytimg.com/vi/{video_id}/hqdefault.jpg",
            f"https://i.ytimg.com/vi/{video_id}/mqdefault.jpg"
        ]
        for u in cdn_urls:
            try:
                req = urllib.request.Request(u, headers={"User-Agent": "Mozilla/5.0"})
                with urllib.request.urlopen(req, timeout=5) as resp:
                    data = resp.read()
                    if len(data) > 1000:
                        if process_and_save_cover(data, dest_mero):
                            last_art_url = cache_key
                            print(f"[+] Processed & deployed HD YouTube cover art: {len(data)}B -> 214x214 square ({u})")
                            return
            except Exception:
                continue

    # 2. Check if local artUrl file exists (e.g. MPRIS cache)
    if art_url.startswith("file://"):
        local_path = Path(art_url.replace("file://", ""))
        if local_path.exists():
            try:
                data = local_path.read_bytes()
                if len(data) > 200:
                    if process_and_save_cover(data, dest_mero):
                        last_art_url = cache_key
                        print(f"[+] Processed & deployed cover art from local file: {local_path.name}")
                        return
            except Exception:
                pass

    # 3. Remote HTTP art_url
    if art_url.startswith("http"):
        try:
            req = urllib.request.Request(art_url, headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=5) as resp:
                data = resp.read()
                if len(data) > 1000:
                    if process_and_save_cover(data, dest_mero):
                        last_art_url = cache_key
                        print(f"[+] Processed & deployed cover art from artUrl ({art_url})")
                        return
        except Exception:
            pass


def stream_now_playing(media):
    global last_stream_send_time
    now = time.time()
    if (now - last_stream_send_time) >= 0.5:
        now_str = time.strftime("%Y-%m-%d %H:%M:%S")
        clean = lambda value: str(value).replace("|", "/").replace("\r", " ").replace("\n", " ")
        line = (
            f"MERO:NOW:status={clean(media['status'])}|title={clean(media['title'])}|"
            f"artist={clean(media['artist'])}|album={clean(media['album'])}|"
            f"pos={media['position']}|len={media['length']}|time={now_str}"
        )
        send_stream(line)
        last_stream_send_time = now


def write_now_playing(sd_mount, media):
    if not sd_mount:
        return

    now_str = time.strftime("%Y-%m-%d %H:%M:%S")
    content = (
        f"title={media['title']}\r\n"
        f"artist={media['artist']}\r\n"
        f"album={media['album']}\r\n"
        f"status={media['status']}\r\n"
        f"position={media['position']}\r\n"
        f"length={media['length']}\r\n"
        f"cover=\\SDMMC\\MERO\\cover.jpg\r\n"
        f"host_time={now_str}\r\n"
    ).encode("utf-8")

    dest_mero = sd_mount / "MERO" / "now_playing.txt"
    try:
        dest_mero.parent.mkdir(parents=True, exist_ok=True)
        # Match the working visualizer transport: overwrite one fixed file.
        with open(dest_mero, "wb") as f:
            f.write(content)
            f.flush()
            os.fsync(f.fileno())
        os.sync()
    except Exception as e:
        print(f"[!] Storage write error: {e}")


# ====================================================================
# MAIN CONTINUOUS PROBING LOOP
# ====================================================================
def main():
    global active_player, last_title, last_status, last_art_url
    sys.stdout.reconfigure(line_buffering=True)
    print("=== MERO COMPANION DECK: CONTINUOUS PROBING DAEMON ===")
    print("  [+] Strict YouTube Music Tracking (music.youtube.com)")
    print("  [+] Cyberpunk Weather Matrix (Farroupilha, RS)")

    sd_mount = find_sd_mount()
    if sd_mount:
        print(f"[+] SD card detected at: {sd_mount}")
        sync_binaries(sd_mount)
        fetch_weather(sd_mount)

    poll_count = 0
    while True:
        try:
            # 1. Mount maintenance
            if poll_count % 20 == 0 or not sd_mount:
                new_mount = find_sd_mount()
                if new_mount != sd_mount:
                    sd_mount = new_mount
                    if sd_mount:
                        print(f"[+] SD card mounted at: {sd_mount}")
                        sync_binaries(sd_mount)
                        fetch_weather(sd_mount)

            # Check for updated binary every 5s
            if poll_count % 10 == 0 and sd_mount:
                sync_binaries(sd_mount)

            # 2. Weather telemetry (every 10 minutes)
            if poll_count % 1200 == 0:
                fetch_weather(sd_mount)

            # 3. Continuous player probing (every 500ms)
            found_player = find_target_player()
            if found_player != active_player:
                active_player = found_player
                if active_player:
                    print(f"[+] Locked onto YouTube Music player: {active_player}")
                else:
                    print("[-] Waiting for YouTube Music (music.youtube.com)...")

            # 4. Media status, live serial streaming, and storage sync
            media = get_media_status(active_player)
            if media:
                if media["title"] != last_title or media["status"] != last_status:
                    if media["title"] != last_title:
                        last_art_url = ""
                    last_title = media["title"]
                    last_status = media["status"]
                    print(f"[*] YouTube Music: {media['artist']} - {media['title']} [{media['status']}] ({media['position']}s / {media['length']}s)")

                # Stream through TCP once ActiveSync PPP is established.
                stream_now_playing(media)

                # Storage sync when SD card is available (Mass Storage mode)
                if sd_mount:
                    update_artwork(media["art_url"], sd_mount, media.get("track_url", ""), media["title"])
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
