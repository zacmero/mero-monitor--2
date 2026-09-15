#!/usr/bin/env bash
set -e

SD_MOUNT="/run/media/zacmero/6232-3562"
DEV_UUID="6232-3562"

echo "=== MERO MONITOR #2: AUTONOMOUS SDMMC DEPLOY WATCHER ==="
echo "[*] Waiting for GPS to switch to USB Mass Storage (UUID: $DEV_UUID)..."

for i in $(seq 1 300); do
    DEV=$(lsblk -rnpo NAME,UUID 2>/dev/null | grep "$DEV_UUID" | awk '{print $1}' | head -n 1)
    if [ -n "$DEV" ]; then
        echo "[+] Detected SD card device at $DEV!"
        sleep 1
        udisksctl mount -b "$DEV" 2>/dev/null || true
        if [ -d "$SD_MOUNT" ]; then
            /home/zacmero/projects/mero-monitor-#2/scripts/deploy_sd.sh
            echo "[+] SUCCESS: Deployed update to SDMMC!"
            exit 0
        fi
    fi
    sleep 1
done

echo "[!] Timed out waiting for SD card."
exit 1
