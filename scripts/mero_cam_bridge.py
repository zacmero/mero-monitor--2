#!/usr/bin/env python3
"""
mero_cam_bridge.py - Host Bridge for Mero Monitor #2 DarkHorse Webcam Deck
Connects DarkHorse USB 2.0 Web Camera (1b17:6101, VC0321/OV7660) to Windows CE companion deck.
Integrates recording, snapshots, v4l2 hardware controls, and real-time 8 kHz whine filtering.
"""

import os
import sys
import time
import shutil
import datetime
import subprocess
from pathlib import Path

DEFAULT_MOUNT = Path("/run/media/zacmero/6232-3562")
DEV_UUID = "6232-3562"
VIDEO_DEV = "/dev/video0"
AUDIO_SRC = "alsa_input.pci-0000_00_1b.0.analog-stereo"
AUDIO_FILTER = "highpass=f=100,firequalizer=gain_entry='entry(0,0);entry(7000,0);entry(7500,-80);entry(24000,-80)',afftdn=nf=-20"

last_processed_seq = -1
last_processed_cmd = ""
is_streaming = False
is_recording = False
is_preview = False
ffmpeg_proc = None
preview_proc = None
rec_output_path = ""
last_client_seen = 0.0

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

def stop_streaming():
    global ffmpeg_proc, is_streaming, is_recording
    if ffmpeg_proc is not None:
        try:
            ffmpeg_proc.terminate()
            ffmpeg_proc.wait(timeout=2)
        except Exception:
            try:
                ffmpeg_proc.kill()
            except Exception:
                pass
        ffmpeg_proc = None
    is_streaming = False
    is_recording = False
    print("[*] Camera stream stopped (hardware released).")

def start_streaming(sd_mount, record=False):
    global ffmpeg_proc, is_streaming, is_recording, rec_output_path
    stop_streaming()

    if not os.path.exists(VIDEO_DEV):
        print(f"[!] Error: {VIDEO_DEV} not connected.")
        return False

    stream_dir = sd_mount / "Stream"
    stream_dir.mkdir(parents=True, exist_ok=True)
    out_cam = str(stream_dir / "cam.jpg")
    tmp_live = "/tmp/mero_cam_live.jpg"

    if record:
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        rec_output_path = os.path.expanduser(f"~/webcam_recording_{ts}.mp4")
        cmd = [
            "ffmpeg", "-loglevel", "error", "-y",
            "-thread_queue_size", "1024", "-f", "v4l2",
            "-input_format", "yuyv422", "-video_size", "636x476", "-i", VIDEO_DEV,
            "-thread_queue_size", "1024", "-f", "pulse", "-i", AUDIO_SRC,
            "-r", "4", "-update", "1", "-atomic_writing", "1", "-q:v", "4", out_cam,
            "-c:v", "libx264", "-pix_fmt", "yuv420p",
            "-af", AUDIO_FILTER,
            "-c:a", "aac", "-b:a", "192k", rec_output_path
        ]
        is_recording = True
        print(f"[+] Recording STARTED -> {rec_output_path}")
    else:
        cmd = [
            "ffmpeg", "-loglevel", "error", "-y",
            "-f", "v4l2", "-input_format", "yuyv422", "-video_size", "636x476", "-i", VIDEO_DEV,
            "-r", "4", "-update", "1", "-atomic_writing", "1", "-q:v", "4", out_cam
        ]
        is_recording = False
        print(f"[+] Stream started -> {out_cam} (4 FPS)")

    try:
        ffmpeg_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        is_streaming = True
        return True
    except Exception as e:
        print(f"[!] Failed to launch ffmpeg: {e}")
        return False

def take_snapshot(sd_mount):
    src = sd_mount / "Stream" / "cam.jpg"
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    dest = os.path.expanduser(f"~/webcam_snapshot_{ts}.jpg")
    if src.exists():
        try:
            shutil.copyfile(src, dest)
            print(f"[+] Snapshot saved -> {dest}")
            return f"Saved: {Path(dest).name}"
        except Exception as e:
            return f"Snap error: {e}"
    return "No frame yet"

def toggle_preview(sd_mount):
    global preview_proc, is_preview
    if preview_proc and preview_proc.poll() is None:
        try:
            preview_proc.terminate()
            preview_proc.wait(timeout=1)
        except Exception:
            try:
                preview_proc.kill()
            except Exception:
                pass
        preview_proc = None
        is_preview = False
        print("[*] Desktop preview closed.")
        return "Preview Closed"
    else:
        cam_file = str(sd_mount / "Stream" / "cam.jpg")
        cmd = [
            "mpv", "--title=DarkHorse Cam Live",
            "--loop=inf", "--fps=4", cam_file
        ]
        try:
            preview_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            is_preview = True
            print("[+] Desktop preview opened.")
            return "Preview Active"
        except Exception as e:
            return f"Preview err: {e}"

def write_status(sd_mount, msg=""):
    if not sd_mount:
        return
    brt = get_v4l2_control("brightness")
    ctr = get_v4l2_control("contrast")
    status_str = "RECORDING" if is_recording else ("STREAMING" if is_streaming else "IDLE")

    payload = (
        f"status={status_str}\r\n"
        f"streaming={1 if is_streaming else 0}\r\n"
        f"recording={1 if is_recording else 0}\r\n"
        f"preview={1 if is_preview else 0}\r\n"
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

def check_commands(sd_mount):
    global last_processed_seq, last_processed_cmd, last_client_seen
    if not sd_mount:
        return

    cmd_files = [
        sd_mount / "Stream" / "cam_cmd.txt",
        sd_mount / "MERO" / "cam_cmd.txt"
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
                last_client_seen = time.time()
                print(f"[+] DISPATCHING CAM COMMAND: '{cmd}' (seq={seq})")
                feedback = ""

                if cmd in ["START", "HEARTBEAT"]:
                    if not is_streaming:
                        start_streaming(sd_mount)
                    feedback = "Stream Live"
                elif cmd == "STOP":
                    stop_streaming()
                    feedback = "Stream Stopped"
                elif cmd == "SNAP":
                    feedback = take_snapshot(sd_mount)
                elif cmd == "REC":
                    if is_recording:
                        start_streaming(sd_mount, record=False)
                        feedback = "Recording Saved"
                    else:
                        start_streaming(sd_mount, record=True)
                        feedback = "Recording..."
                elif cmd == "PREVIEW":
                    feedback = toggle_preview(sd_mount)
                elif cmd == "BRT_UP":
                    cur = get_v4l2_control("brightness")
                    set_v4l2_control("brightness", cur + 10)
                    feedback = f"Brightness: {get_v4l2_control('brightness')}"
                elif cmd == "BRT_DN":
                    cur = get_v4l2_control("brightness")
                    set_v4l2_control("brightness", cur - 10)
                    feedback = f"Brightness: {get_v4l2_control('brightness')}"
                elif cmd == "CTR_UP":
                    cur = get_v4l2_control("contrast")
                    set_v4l2_control("contrast", cur + 10)
                    feedback = f"Contrast: {get_v4l2_control('contrast')}"
                elif cmd == "CTR_DN":
                    cur = get_v4l2_control("contrast")
                    set_v4l2_control("contrast", cur - 10)
                    feedback = f"Contrast: {get_v4l2_control('contrast')}"

                write_status(sd_mount, feedback)

                ack_text = f"seq={seq or 0}\ncmd=PROCESSED\n"
                for cf in cmd_files:
                    if cf.exists():
                        try:
                            cf.write_text(ack_text, encoding="utf-8")
                        except Exception:
                            pass
        except Exception as e:
            print(f"[!] Error processing {cmd_file}: {e}")

def main():
    global last_client_seen
    sys.stdout.reconfigure(line_buffering=True)
    print("=== MERO MONITOR #2: DARKHORSE WEBCAM BRIDGE ===")
    print(f"[*] Monitoring device: {VIDEO_DEV} (VC0321/OV7660)")
    print("[*] Ready for commands from Windows CE companion deck...")

    sd_mount = find_sd_mount()
    if sd_mount:
        # Ignore any stale commands written before bridge startup
        for cf in [sd_mount / "Stream" / "cam_cmd.txt", sd_mount / "MERO" / "cam_cmd.txt"]:
            if cf.exists():
                try:
                    for l in cf.read_text().splitlines():
                        if l.startswith("seq="):
                            last_processed_seq = max(last_processed_seq, int(l.split("=")[1]))
                except Exception:
                    pass

        # Immediately activate camera so user sees stream right away upon starting app
        start_streaming(sd_mount)
        write_status(sd_mount, "Camera Ready")
        last_client_seen = time.time()

    poll_count = 0
    while True:
        try:
            if poll_count % 10 == 0 or not sd_mount:
                sd_mount = find_sd_mount()

            if sd_mount:
                check_commands(sd_mount)

            # Auto-idle if client inactive for > 120 seconds
            if is_streaming and (time.time() - last_client_seen > 120):
                print("[*] Client inactive for 120s, stopping stream to release camera.")
                stop_streaming()
                if sd_mount:
                    write_status(sd_mount, "Standby (Idle)")

            poll_count += 1
            time.sleep(0.12)
        except KeyboardInterrupt:
            break
        except Exception as e:
            time.sleep(0.3)

    stop_streaming()
    if preview_proc:
        try:
            preview_proc.terminate()
        except Exception:
            pass

if __name__ == "__main__":
    main()
