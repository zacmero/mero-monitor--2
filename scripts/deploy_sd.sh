#!/usr/bin/env bash
set -e

SD_MOUNT="/run/media/zacmero/6232-3562"
DEV_UUID="6232-3562"

echo "=== MERO MONITOR #2: SDMMC DEPLOYMENT ==="

# Check if mountpoint is present or find device by UUID
if [ ! -d "$SD_MOUNT" ]; then
    DEV_PATH=$(blkid -U "$DEV_UUID" 2>/dev/null || true)
    if [ -n "$DEV_PATH" ]; then
        echo "[+] Found SD card device at $DEV_PATH, mounting..."
        udisksctl mount -b "$DEV_PATH" || true
    fi
fi

if [ ! -d "$SD_MOUNT" ]; then
    echo "[!] SD card mount ($SD_MOUNT) is not currently accessible."
    echo "[!] Please switch device to USB MASS STORAGE mode or connect SD card."
    exit 1
fi

echo "[+] Target SDMMC directory: $SD_MOUNT"
mkdir -p "$SD_MOUNT/MERO"
mkdir -p "$SD_MOUNT/Stream"

echo "[+] Copying latest compiled WinCE binaries..."
cp -v wince/shell/mero-shell.exe "$SD_MOUNT/MERO/mero-shell.exe"
cp -v wince/gallery/mero-gallery.exe "$SD_MOUNT/MERO/mero-gallery.exe"
[ -f wince/cmd/mero-cmd.exe ] && cp -v wince/cmd/mero-cmd.exe "$SD_MOUNT/MERO/mero-cmd.exe"
[ -f wince/probe/mero-probe.exe ] && cp -v wince/probe/mero-probe.exe "$SD_MOUNT/MERO/mero-probe.exe"
[ -f wince/flash/mero-flash.exe ] && cp -v wince/flash/mero-flash.exe "$SD_MOUNT/MERO/mero-flash.exe"

for ico in wince/shell/*.ico; do
    [ -f "$ico" ] && cp -v "$ico" "$SD_MOUNT/MERO/"
done

echo "[+] Flushing filesystem cache..."
sync
echo "[+] DEPLOYMENT COMPLETE: All binaries updated on SD card."
