#!/usr/bin/env bash
set -euo pipefail
if (( EUID != 0 )); then
  printf 'Run with sudo to install the system USB transport service.\n' >&2
  exit 1
fi
repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
command -v pppd >/dev/null
command -v chat >/dev/null
if systemctl is-active --quiet mero-ppp.service; then
  printf 'Stop the existing mero-ppp.service before installing; one PPP owner only.\n' >&2
  exit 1
fi
if [[ -e /etc/systemd/system/mero-sync.service ]]; then
  cp -a /etc/systemd/system/mero-sync.service "/etc/systemd/system/mero-sync.service.previous-$(date +%Y%m%d-%H%M%S)-$$"
fi
# Escape percent specifiers and use a quoted argument for paths containing #.
escaped_repo="${repo_dir//%/%%}"
escaped_repo="${escaped_repo//\\/\\\\}"
escaped_repo="${escaped_repo//\"/\\\"}"
sed "\\|^ExecStart=|d" "$repo_dir/host/bridge/mero-sync.service" > /etc/systemd/system/mero-sync.service
sed -i "/^Type=simple/a ExecStart=/usr/bin/python3 \"${escaped_repo}/host/bridge/mero_sync.py\"" /etc/systemd/system/mero-sync.service
systemctl daemon-reload
systemctl enable --now mero-sync.service
