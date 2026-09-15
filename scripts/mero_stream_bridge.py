#!/usr/bin/env python3
"""
mero-stream-bridge.py - Host Workstation to WinCE Live Stream Bridge
Project: mero-monitor-#2
Author: Zac Mero / Antigravity

Watches a host directory (e.g. ~/Desktop/1_Projects/Suzy Mero Steele/stream_gallery/)
and automatically optimizes, transcodes, and streams all images and videos
directly to the connected WinCE device's \\SDMMC\\Stream folder.
"""

import os
import sys
import time
import glob
import shutil
import argparse
import subprocess
from pathlib import Path
from PIL import Image

TARGET_WIDTH = 480
TARGET_HEIGHT = 272
SUPPORTED_IMG_EXTS = {'.png', '.webp', '.jpg', '.jpeg', '.bmp', '.gif', '.tiff'}
SUPPORTED_VID_EXTS = {'.mp4', '.mov', '.avi', '.mkv', '.webm', '.flv', '.wmv'}

def find_default_destination():
    """Auto-detect mounted SD card partition under /run/media."""
    media_base = "/run/media"
    if not os.path.exists(media_base):
        return None
    for root, dirs, _ in os.walk(media_base):
        for d in dirs:
            p = os.path.join(root, d)
            # Check if this looks like the Foston SDMMC (has MERO or shell.ini)
            if os.path.exists(os.path.join(p, "MERO")) or os.path.exists(os.path.join(p, "shell.ini")):
                stream_dir = os.path.join(p, "Stream")
                os.makedirs(stream_dir, exist_ok=True)
                return stream_dir
    # Fallback to known mount path
    known = "/run/media/zacmero/6232-3562/Stream"
    if os.path.exists(os.path.dirname(known)):
        os.makedirs(known, exist_ok=True)
        return known
    return None

def process_image(src_path, dst_path):
    """
    Load image (WebP, PNG, JPG, etc.), fit to 480x272 with letterbox,
    and save as optimized baseline JPEG.
    """
    try:
        with Image.open(src_path) as im:
            im = im.convert('RGB')
            # Calculate thumbnail dimensions preserving aspect ratio
            im_copy = im.copy()
            im_copy.thumbnail((TARGET_WIDTH, TARGET_HEIGHT), Image.Resampling.LANCZOS)
            
            # Create centered letterbox canvas
            canvas = Image.new('RGB', (TARGET_WIDTH, TARGET_HEIGHT), (0, 0, 0))
            offset_x = (TARGET_WIDTH - im_copy.width) // 2
            offset_y = (TARGET_HEIGHT - im_copy.height) // 2
            canvas.paste(im_copy, (offset_x, offset_y))
            
            # Write to temp file then rename for atomic write
            tmp_path = dst_path + ".tmp"
            canvas.save(tmp_path, "JPEG", quality=92, optimize=True)
            os.replace(tmp_path, dst_path)
            print(f"[STREAM-IMG] {os.path.basename(src_path)} -> {os.path.basename(dst_path)} ({os.path.getsize(dst_path)}B)")
            return True
    except Exception as e:
        print(f"[ERROR-IMG] Failed to process {src_path}: {e}", file=sys.stderr)
        return False

def process_video(src_path, dst_dir, base_name, fps=1.0):
    """
    Extract video frames via ffmpeg, downsample to 480x272 letterboxed JPEGs.
    """
    try:
        # Check ffmpeg presence
        if not shutil.which("ffmpeg"):
            print("[WARN] ffmpeg not found; cannot process video.", file=sys.stderr)
            return []

        out_pattern = os.path.join(dst_dir, f"{base_name}_fr%03d.jpg")
        vf_filter = f"fps={fps},scale={TARGET_WIDTH}:{TARGET_HEIGHT}:force_original_aspect_ratio=decrease,pad={TARGET_WIDTH}:{TARGET_HEIGHT}:(ow-iw)/2:(oh-ih)/2:black"
        
        cmd = [
            "ffmpeg", "-y", "-loglevel", "error",
            "-i", src_path,
            "-vf", vf_filter,
            "-q:v", "3",
            out_pattern
        ]
        subprocess.run(cmd, check=True)
        
        # Collect created frame files
        created = sorted(glob.glob(os.path.join(dst_dir, f"{base_name}_fr*.jpg")))
        print(f"[STREAM-VID] {os.path.basename(src_path)} -> {len(created)} frames ({fps} fps)")
        return created
    except Exception as e:
        print(f"[ERROR-VID] Failed to process video {src_path}: {e}", file=sys.stderr)
        return []

def sync_stream_directory(src_dir, dst_dir, video_fps=1.0):
    """
    Synchronize src_dir to dst_dir:
    - Transcode images to 480x272 JPEGs
    - Extract video frames to 480x272 JPEGs
    - Remove stale files in dst_dir
    - Call sync to flush USB buffers
    """
    if not os.path.exists(src_dir):
        print(f"[WARN] Source directory does not exist: {src_dir}")
        return False

    os.makedirs(dst_dir, exist_ok=True)
    active_dst_files = set()
    changed = False

    src_entries = sorted(os.listdir(src_dir))
    for entry in src_entries:
        src_path = os.path.join(src_dir, entry)
        if not os.path.isfile(src_path):
            continue

        ext = os.path.splitext(entry)[1].lower()
        stem = os.path.splitext(entry)[0]
        # Clean stem for FAT32 compatibility (no spaces, special chars)
        safe_stem = "".join(c if c.isalnum() or c in ('_', '-') else '_' for c in stem)[:28]

        if ext in SUPPORTED_IMG_EXTS:
            out_name = f"{safe_stem}.jpg"
            out_path = os.path.join(dst_dir, out_name)
            active_dst_files.add(out_name)

            src_mtime = os.path.getmtime(src_path)
            dst_mtime = os.path.getmtime(out_path) if os.path.exists(out_path) else 0

            if src_mtime > dst_mtime:
                if process_image(src_path, out_path):
                    changed = True

        elif ext in SUPPORTED_VID_EXTS:
            # Check if any frames already exist and are newer than video
            existing_frames = sorted(glob.glob(os.path.join(dst_dir, f"{safe_stem}_fr*.jpg")))
            src_mtime = os.path.getmtime(src_path)
            rebuild_video = False

            if not existing_frames:
                rebuild_video = True
            else:
                oldest_frame_mtime = min(os.path.getmtime(f) for f in existing_frames)
                if src_mtime > oldest_frame_mtime:
                    rebuild_video = True

            if rebuild_video:
                # Remove existing frames first
                for f in existing_frames:
                    try:
                        os.remove(f)
                    except OSError:
                        pass
                frames = process_video(src_path, dst_dir, safe_stem, fps=video_fps)
                if frames:
                    changed = True
                    for f in frames:
                        active_dst_files.add(os.path.basename(f))
            else:
                for f in existing_frames:
                    active_dst_files.add(os.path.basename(f))

    # Clean up obsolete files in destination directory
    for f in os.listdir(dst_dir):
        if f.lower().endswith(('.jpg', '.jpeg', '.bmp')) and f not in active_dst_files:
            try:
                os.remove(os.path.join(dst_dir, f))
                print(f"[STREAM-PRUNE] Removed stale file: {f}")
                changed = True
            except OSError:
                pass

    if changed:
        try:
            os.sync()
            print("[SYNC] Flushed storage buffers to USB device.")
        except AttributeError:
            pass

    return changed

def main():
    parser = argparse.ArgumentParser(description="Mero Monitor #2 Host Stream Bridge")
    parser.add_argument("--src", default="/home/zacmero/Desktop/1_Projects/Suzy Mero Steele/stream_gallery",
                        help="Host source directory containing wallpapers and media")
    parser.add_argument("--dst", default=None,
                        help="Target directory on device SD card (auto-detected if omitted)")
    parser.add_argument("--fps", type=float, default=1.0,
                        help="Video frame extraction rate in fps (default: 1.0)")
    parser.add_argument("--watch", action="store_true",
                        help="Run continuously in watch mode, monitoring source for changes")
    parser.add_argument("--interval", type=float, default=2.0,
                        help="Watch polling interval in seconds (default: 2.0)")

    args = parser.parse_args()

    dst_dir = args.dst or find_default_destination()
    if not dst_dir:
        print("[FATAL] Could not find mounted WinCE SD card volume under /run/media.", file=sys.stderr)
        print("Please specify destination manually with --dst /path/to/Stream", file=sys.stderr)
        sys.exit(1)

    print(f"=== MERO MONITOR #2 STREAM BRIDGE ===")
    print(f"Source:      {args.src}")
    print(f"Destination: {dst_dir}")
    print(f"Video FPS:   {args.fps}")
    print(f"=====================================")

    sync_stream_directory(args.src, dst_dir, video_fps=args.fps)

    if args.watch:
        print(f"[WATCH] Monitoring '{args.src}' every {args.interval}s. Press Ctrl+C to stop.")
        try:
            while True:
                time.sleep(args.interval)
                # Re-verify destination still exists (in case remounted)
                if not os.path.exists(dst_dir):
                    redetect = find_default_destination()
                    if redetect:
                        dst_dir = redetect
                    else:
                        continue
                sync_stream_directory(args.src, dst_dir, video_fps=args.fps)
        except KeyboardInterrupt:
            print("\n[WATCH] Stopped by user.")

if __name__ == "__main__":
    main()
