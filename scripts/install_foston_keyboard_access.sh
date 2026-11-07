#!/usr/bin/env bash
# One-time persistent permission setup; no password/secret is saved.
set -euo pipefail
if [[ "$(id -u)" != 0 ]]; then
    echo 'Run this setup once with sudo. Normal Foston keyboard use needs no sudo.' >&2
    exit 1
fi
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_rule="$script_dir/../host/70-mero-foston-keyboard.rules"
keyboard_vendor="${1:-2a7a}"
keyboard_product="${2:-9005}"
if [[ ! "$keyboard_vendor" =~ ^[[:xdigit:]]{4}$ || ! "$keyboard_product" =~ ^[[:xdigit:]]{4}$ ]]; then
    echo 'Optional arguments must be four-digit USB vendor and product IDs.' >&2
    exit 1
fi
keyboard_vendor="${keyboard_vendor,,}"
keyboard_product="${keyboard_product,,}"
rule_tmp="$(mktemp --suffix=.rules)"
trap 'rm -f -- "$rule_tmp"' EXIT
sed -e "s/2a7a/$keyboard_vendor/g" -e "s/9005/$keyboard_product/g" "$source_rule" > "$rule_tmp"
udevadm verify "$rule_tmp"
target_rule=/etc/udev/rules.d/70-mero-foston-keyboard.rules
if [[ -f "$target_rule" ]] && ! cmp -s "$rule_tmp" "$target_rule"; then
    cp -p -- "$target_rule" "$target_rule.bak-$(date -u +%Y%m%dT%H%M%S%N)"
fi
install -m 0644 -- "$rule_tmp" "$target_rule"
udevadm control --reload-rules
udevadm trigger --action=change --subsystem-match=input \
    --property-match="ID_VENDOR_ID=$keyboard_vendor" --property-match="ID_MODEL_ID=$keyboard_product"
udevadm settle --timeout=5
echo 'Persistent keyboard access installed. Reboots and reconnects need no sudo.'
