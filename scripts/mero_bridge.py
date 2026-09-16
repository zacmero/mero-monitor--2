#!/usr/bin/env python3
"""
mero_bridge.py - Unified Host Bridge for Mero Monitor #2
Single unified companion daemon for Windows CE deck:
  1. Media Deck: YouTube Music / MPRIS playback, metadata, and volume control
  2. DarkHorse Webcam: On-demand video streaming (scaled 480x272), snapshots,
     whine-filtered recording (FIR notch filter), desktop preview, and v4l2 tuning.
"""

import os
import sys
import time
import shutil
import select
import termios
import datetime
import urllib.request
import subprocess
from pathlib import Path

DEFAULT_MOUNT = Path("/run/media/zacmero/6232-3562")
DEV_UUID = "6232-3562"
SERIAL_PORT = "/dev/ttyUSB0"
VIDEO_DEV = "/dev/video0"
AUDIO_SRC = "alsa_input.pci-0000_00_1b.0.analog-stereo"
AUDIO_FILTER = "highpass=f=100,firequalizer=gain_entry='entry(0,0);entry(7000,0);entry(7500,-80);entry(24000,-80)',afftdn=nf=-20"

# Media state
last_art_url = ""
last_title = ""
last_status = ""
active_player = ""
last_media_seq = -1
last_media_cmd = ""
serial_fd = None
rx_serial_buf = ""
last_serial_send_time = 0.0
_last_now_playing_content = b""

# Camera state
last_cam_seq = -1
last_cam_cmd = ""
is_cam_streaming = False
is_cam_recording = False
is_cam_preview = False
cam_ffmpeg_proc = None
cam_preview_proc = None
last_cam_heartbeat = 0.0

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
                    if len(data) > 500:
                        tmp_raw.write_bytes(data)
                        fetched = True
                        break
            except Exception:
                continue

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
                mero_dir = sd_mount / "MERO"
                mero_dir.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(tmp_out, mero_dir / "cover.jpg")

                # Purge any cover.jpg in Stream so slideshow is NEVER polluted
                stream_cover = sd_mount / "Stream" / "cover.jpg"
                if stream_cover.exists():
                    try:
                        stream_cover.unlink()
                    except Exception:
                        pass

                last_art_url = cache_key
                src = "YT CDN" if video_id else "MPRIS"
                print(f"[+] Album art updated ({src}, vid={video_id or 'n/a'}) -> MERO/cover.jpg")
    except Exception as e:
        print(f"[!] Artwork fetch error: {e}")

def execute_transport_command(cmd, player):
    p_flag = f'-p "{player}" ' if player else ""
    print(f"[+] MEDIA CMD: '{cmd}' -> Player: {player}")
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

def check_media_storage_commands(sd_mount, player):
    global last_media_seq, last_media_cmd
    if not sd_mount:
        return

    cmd_files = [sd_mount / "MERO" / "media_cmd.txt", sd_mount / "Stream" / "media_cmd.txt"]
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
                if seq != last_media_seq:
                    is_new = True
                    last_media_seq = seq
            else:
                if cmd != last_media_cmd:
                    is_new = True
                    last_media_cmd = cmd

            if is_new:
                execute_transport_command(cmd, player)
                for cf in cmd_files:
                    if cf.exists():
                        try:
                            cf.write_text(f"seq={seq or 0}\ncmd=PROCESSED\n", encoding="utf-8")
                        except Exception:
                            pass
        except Exception as e:
            print(f"[!] Media command error: {e}")

def write_now_playing(sd_mount, media):
    global _last_now_playing_content
    if not sd_mount or not media:
        return

    content = (
        f"title={media['title']}\r\n"
        f"artist={media['artist']}\r\n"
        f"album={media['album']}\r\n"
        f"status={media['status']}\r\n"
        f"position={media['position']}\r\n"
        f"length={media['length']}\r\n"
        f"cover=\\SDMMC\\MERO\\cover.jpg\r\n"
    ).encode("utf-8")

    if content == _last_now_playing_content:
        return
    _last_now_playing_content = content

    for target in [sd_mount / "Stream" / "now_playing.txt", sd_mount / "MERO" / "now_playing.txt"]:
        try:
            fd = os.open(str(target), os.O_WRONLY | os.O_CREAT | os.O_SYNC, 0o666)
            os.lseek(fd, 0, os.SEEK_SET)
            os.write(fd, content)
            os.ftruncate(fd, len(content))
            os.close(fd)
        except Exception:
            pass

# ====================================================================
# DARKHORSE WEBCAM CONTROLLER (On-Demand)
# ====================================================================
def get_v4l2_control(name):
    out = run_cmd(f"v4l2-ctl -d {VIDEO_DEV} --get-ctrl={name} 2>/dev/null")
    if ":" in out:
        try:
            return int(out.split(":")[1].strip())
        except ValueError:
            pass
    return 128

def set_v4l2_control(name, val):
    val = max(0, min(255, val))
    run_cmd(f"v4l2-ctl -d {VIDEO_DEV} --set-ctrl={name}={val} 2>/dev/null")
    return val

def stop_cam_stream():
    global cam_ffmpeg_proc, is_cam_streaming, is_cam_recording
    if cam_ffmpeg_proc is not None:
        try:
            cam_ffmpeg_proc.terminate()
            cam_ffmpeg_proc.wait(timeout=2)
        except Exception:
            try:
                cam_ffmpeg_proc.kill()
            except Exception:
                pass
        cam_ffmpeg_proc = None
    is_cam_streaming = False
    is_cam_recording = False
    print("[*] DarkHorse camera stream stopped (hardware released).")

def start_cam_stream(sd_mount, record=False):
    global cam_ffmpeg_proc, is_cam_streaming, is_cam_recording
    stop_cam_stream()

    if not os.path.exists(VIDEO_DEV):
        print(f"[!] Error: {VIDEO_DEV} not connected.")
        return False

    stream_dir = sd_mount / "Stream"
    stream_dir.mkdir(parents=True, exist_ok=True)
    out_cam = str(stream_dir / "cam.jpg")

    # Native capture 636x476 scaled directly on host CPU to 480x272 TFT LCD geometry!
    # WinCE decodes 480x272 directly with zero floating-point software scaling!
    if record:
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        rec_path = os.path.expanduser(f"~/webcam_recording_{ts}.mp4")
        cmd = [
            "ffmpeg", "-loglevel", "error", "-y",
            "-thread_queue_size", "1024", "-f", "v4l2",
            "-input_format", "yuyv422", "-video_size", "636x476", "-i", VIDEO_DEV,
            "-thread_queue_size", "1024", "-f", "pulse", "-i", AUDIO_SRC,
            "-vf", "scale=480:272", "-r", "4", "-update", "1", "-atomic_writing", "1", "-q:v", "3", out_cam,
            "-c:v", "libx264", "-pix_fmt", "yuv420p",
            "-af", AUDIO_FILTER,
            "-c:a", "aac", "-b:a", "192k", rec_path
        ]
        is_cam_recording = True
        print(f"[+] Cam Recording STARTED -> {rec_path}")
    else:
        cmd = [
            "ffmpeg", "-loglevel", "error", "-y",
            "-f", "v4l2", "-input_format", "yuyv422", "-video_size", "636x476", "-i", VIDEO_DEV,
            "-vf", "scale=480:272", "-r", "4", "-update", "1", "-atomic_writing", "1", "-q:v", "3", out_cam
        ]
        is_cam_recording = False
        print(f"[+] Cam Stream STARTED -> {out_cam} (480x272 @ 4 FPS)")

    try:
        cam_ffmpeg_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        is_cam_streaming = True
        return True
    except Exception as e:
        print(f"[!] Failed to launch camera ffmpeg: {e}")
        return False

def take_cam_snapshot(sd_mount):
    src = sd_mount / "Stream" / "cam.jpg"
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    dest = os.path.expanduser(f"~/webcam_snapshot_{ts}.jpg")
    if src.exists():
        try:
            shutil.copyfile(src, dest)
            print(f"[+] Snapshot saved -> {dest}")
            return f"Snap: {Path(dest).name}"
        except Exception as e:
            return f"Snap err: {e}"
    return "No frame yet"

def toggle_cam_preview(sd_mount):
    global cam_preview_proc, is_cam_preview
    if cam_preview_proc and cam_preview_proc.poll() is None:
        try:
            cam_preview_proc.terminate()
            cam_preview_proc.wait(timeout=1)
        except Exception:
            try:
                cam_preview_proc.kill()
            except Exception:
                pass
        cam_preview_proc = None
        is_cam_preview = False
        print("[*] Desktop preview closed.")
        return "Preview Closed"
    else:
        cam_file = str(sd_mount / "Stream" / "cam.jpg")
        cmd = ["mpv", "--title=DarkHorse Cam Live", "--loop=inf", "--fps=4", cam_file]
        try:
            cam_preview_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            is_cam_preview = True
            print("[+] Desktop preview opened.")
            return "Preview Active"
        except Exception as e:
            return f"Preview err: {e}"

def write_cam_status(sd_mount, msg=""):
    if not sd_mount:
        return
    brt = get_v4l2_control("brightness")
    ctr = get_v4l2_control("contrast")
    status_str = "RECORDING" if is_cam_recording else ("STREAMING" if is_cam_streaming else "IDLE")

    payload = (
        f"status={status_str}\r\n"
        f"streaming={1 if is_cam_streaming else 0}\r\n"
        f"recording={1 if is_cam_recording else 0}\r\n"
        f"preview={1 if is_cam_preview else 0}\r\n"
        f"brightness={brt}\r\n"
        f"contrast={ctr}\r\n"
        f"msg={msg}\r\n"
    ).encode("utf-8")

    for target in [sd_mount / "Stream" / "cam_status.txt", sd_mount / "MERO" / "cam_status.txt"]:
        try:
            fd = os.open(str(target), os.O_WRONLY | os.O_CREAT | os.O_SYNC, 0o666)
            os.lseek(fd, 0, os.SEEK_SET)
            os.write(fd, payload)
            os.ftruncate(fd, len(payload))
            os.close(fd)
        except Exception:
            pass

def check_cam_commands(sd_mount):
    global last_cam_seq, last_cam_cmd, last_cam_heartbeat
    if not sd_mount:
        return

    cmd_files = [sd_mount / "Stream" / "cam_cmd.txt", sd_mount / "MERO" / "cam_cmd.txt"]
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
                if seq != last_cam_seq:
                    is_new = True
                    last_cam_seq = seq
            else:
                if cmd != last_cam_cmd:
                    is_new = True
                    last_cam_cmd = cmd

            if is_new:
                last_cam_heartbeat = time.time()
                print(f"[+] CAM CMD: '{cmd}' (seq={seq})")
                feedback = ""

                if cmd in ["START", "HEARTBEAT"]:
                    if not is_cam_streaming:
                        start_cam_stream(sd_mount)
                    feedback = "Live Stream"
                elif cmd == "STOP":
                    stop_cam_stream()
                    feedback = "Stream Stopped"
                elif cmd == "SNAP":
                    feedback = take_cam_snapshot(sd_mount)
                elif cmd == "REC":
                    if is_cam_recording:
                        start_cam_stream(sd_mount, record=False)
                        feedback = "Recording Saved"
                    else:
                        start_cam_stream(sd_mount, record=True)
                        feedback = "Recording..."
                elif cmd == "PREVIEW":
                    feedback = toggle_cam_preview(sd_mount)
                elif cmd == "BRT_UP":
                    cur = get_v4l2_control("brightness")
                    set_v4l2_control("brightness", cur + 10)
                    feedback = f"Brt: {get_v4l2_control('brightness')}"
                elif cmd == "BRT_DN":
                    cur = get_v4l2_control("brightness")
                    set_v4l2_control("brightness", cur - 10)
                    feedback = f"Brt: {get_v4l2_control('brightness')}"
                elif cmd == "CTR_UP":
                    cur = get_v4l2_control("contrast")
                    set_v4l2_control("contrast", cur + 10)
                    feedback = f"Ctr: {get_v4l2_control('contrast')}"
                elif cmd == "CTR_DN":
                    cur = get_v4l2_control("contrast")
                    set_v4l2_control("contrast", cur - 10)
                    feedback = f"Ctr: {get_v4l2_control('contrast')}"

                write_cam_status(sd_mount, feedback)

                for cf in cmd_files:
                    if cf.exists():
                        try:
                            cf.write_text(f"seq={seq or 0}\ncmd=PROCESSED\n", encoding="utf-8")
                        except Exception:
                            pass
        except Exception as e:
            print(f"[!] Cam command error: {e}")

# ====================================================================
# MAIN BRIDGE LOOP
# ====================================================================
def main():
    global active_player, last_title, last_status, last_art_url
    global last_cam_seq, last_media_seq, last_cam_heartbeat
    sys.stdout.reconfigure(line_buffering=True)
    print("=== MERO MONITOR #2: UNIFIED HOST BRIDGE ===")
    print("[*] Managing YouTube Music + DarkHorse Webcam on-demand")

    sd_mount = find_sd_mount()
    if sd_mount:
        print(f"[+] SD card mounted at {sd_mount}")
        # Initialize command sequences to ignore stale files
        for cf in [sd_mount / "Stream" / "cam_cmd.txt", sd_mount / "MERO" / "cam_cmd.txt"]:
            if cf.exists():
                try:
                    for l in cf.read_text().splitlines():
                        if l.startswith("seq="):
                            last_cam_seq = max(last_cam_seq, int(l.split("=")[1]))
                except Exception:
                    pass

    poll_count = 0
    while True:
        try:
            # 1. Mount maintenance
            if poll_count % 15 == 0 or not sd_mount:
                sd_mount = find_sd_mount()

            # 2. Camera on-demand management
            if sd_mount:
                check_cam_commands(sd_mount)

            # Auto-idle camera if app closed (no heartbeat for > 35s)
            if is_cam_streaming and (time.time() - last_cam_heartbeat > 35):
                print("[*] Cam client idle for 35s, releasing camera hardware.")
                stop_cam_stream()
                if sd_mount:
                    write_cam_status(sd_mount, "Standby")

            # 3. Media Player management
            if poll_count % 5 == 0 or not active_player:
                found_player = find_target_player()
                if found_player != active_player:
                    active_player = found_player
                    print(f"[+] Locked onto media player: {active_player}")

            if sd_mount:
                check_media_storage_commands(sd_mount, active_player)

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
            time.sleep(0.12)
        except KeyboardInterrupt:
            break
        except Exception as e:
            time.sleep(0.2)

    stop_cam_stream()
    if cam_preview_proc:
        try:
            cam_preview_proc.terminate()
        except Exception:
            pass

if __name__ == "__main__":
    main()
