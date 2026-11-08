# Foston companion installation on another machine

Start with AGENTS.md and docs/NEXT_SESSION.md. This is a native Windows CE companion controlled by a Linux Bluetooth bridge; Docker is only the ARM build environment. Preserve the resident-flash recovery launcher, navigation assets, agent key/counter and device journals. Do not assume a saved pairing means a working link.

## Host and device baseline

The host needs the existing Bluetooth FTP and companion bridge setup, Python3 with Pillow, tmux, Bash, fastfetch and libxkbcommon. See the repository's Bluetooth/agent operating contract for provisioning and service setup. On an existing provisioned device, do not create a second key, reset counters or casually change the Bluetooth adapter. The keyed build records its host adapter; moving to a different machine/adapter needs an explicit private provisioning/recovery workflow.

Keep the Foston externally powered and in ActiveSync/PC Link. Mass Storage reserves its card and prevents Bluetooth writes. Verify a fresh signed `foston_agent.py status` response before deployment. Use backed-up PUT, exact readback and the protected agent `upgrade` workflow. Never publish the keyed agent executable or private evidence.

## Permanent keyboard access — one-time setup

Normal Mero Terminal keyboard control requires **no sudo and no extra Hyprland window**. The existing bridge performs background forwarding. Linux must grant access to the selected physical keyboard once during installation.

For the current CASUE USB keyboard (vendor2a7a/product9005), from the repository run:

```sh
sudo bash scripts/install_foston_keyboard_access.sh
```

The script installs `/etc/udev/rules.d/70-mero-foston-keyboard.rules`, reloads rules and reapplies them to matching input devices. The rule matches keyboard interfaces of that exact vendor/product only and adds `uaccess`. logind grants access to the active local seat user. Because the rule is persistent, it applies after reboot, USB reconnect and changed `/dev/input/eventN` numbers. It does not add the user to the broad input group, install a privileged forwarding daemon, save passwords or change Foston firmware. An existing differing rule is backed up before replacement.

The initial troubleshooting command `sudo setfacl ... /dev/input/event2 /dev/input/event5` was temporary. Do not use fixed event numbers as the normal installation solution.

For a different keyboard, identify its `ID_VENDOR_ID` and `ID_MODEL_ID` with `udevadm info --query=property --name=/dev/input/eventN`. Pass the four-digit IDs to the installer, for example:

```sh
sudo bash scripts/install_foston_keyboard_access.sh VENDOR PRODUCT
```

Replace VENDOR and PRODUCT with the actual hexadecimal IDs. Then save stable keyboard-interface paths in `~/.config/mero/foston-keyboard.json`:

```json
{"devices":["/dev/input/by-id/YOUR-KEYBOARD-event-kbd"]}
```

List every keyboard interface of that device that must be forwarded. With no configuration, the bridge selects the known CASUE keyboard's stable by-id paths. Restart `mero-bridge.service` after changing its configuration. This keeps access scoped to the user's chosen keyboard on each machine.

Verify the rule and current access:

```sh
rtk proxy udevadm verify host/70-mero-foston-keyboard.rules
rtk proxy udevadm info --query=property --name=/dev/input/eventN
rtk proxy getfacl /dev/input/eventN
```

Use the current event node in the last two commands. Confirm `uaccess` in tags and an ACL for the active user. The rule requires an active local logind seat. A remote/headless session should not silently obtain global keyboard access.

## Using and verifying Mero Terminal

Open **01 – COMPANION LABS → Mero Terminal**. It starts a dedicated persistent tmux/Bash session and compact ArchMerOS fastfetch. Tap **PC KEYS**: the digital keyboard disappears and the physical keyboard goes to the foreground supported Foston app. No desktop app opens. Tap **PC OFF**: normal PC input returns and the digital keyboard stays hidden. Use **SHOW KB / HIDE KB** explicitly when needed. The mouse remains local to the PC. The bridge releases keyboard grabs on errors, lost input acknowledgements, desktop lock/inactivity or process exit; Ctrl+Alt+Escape is an emergency release.

Verify touch typing with a harmless command, then PC typing and mode release. Check that the same tmux pane survives a bridge restart. Do not infer rendering success only from a successful process launch; use the screen capture helper and user confirmation. Actual physical key generation and power cycles still need human interaction.

Detailed controls and current limits: [MERO_TERMINAL.md](MERO_TERMINAL.md). New stable startup ID17 leaves older saved defaults unchanged. Keyboard input uses the active Hyprland layout via libxkbcommon; output uses a bounded ASCII grid. This is a Bluetooth-polled terminal, not a video-rate desktop mirror.

On mero-machine, this permanent rule was installed and user-confirmed on2026-11-07. Agent inspection verified the installed rule, uaccess tags and user ACLs on both CASUE interfaces. Installation on another machine must repeat this one-time setup for that machine's keyboard and seat.

## Foston terminal shortcut (ArchMerOS)

The tracked ArchMerOS Hyprland Lua and legacy configs bind **Super+Alt+Insert** to `~/.config/archmeros/scripts/archmeros-foston-terminal.sh`. Keep the ArchMerOS scripts symlink and the Foston repository clone installed. The wrapper defaults to `$HOME/projects/mero-monitor-#2`; override `MERO_FOSTON_REPO` for another location, then reload Hyprland. Existing one-time keyboard udev permissions cover this feature; normal use needs no sudo.

Activation uses the signed agent to open the named terminal app, waits for fresh device foreground confirmation and all shortcut keys to be released, then grabs the selected keyboard. The same chord releases it inside the capture path, which is necessary because Hyprland cannot see grabbed keys. Ctrl+Alt+Escape also releases it. PC OFF on the device returns keyboard control; SHOW KB / HIDE KB controls its otherwise-hidden touch keyboard. No host terminal/forwarding window is opened.


## Preservation before recovery changes
Read [RECOVERY.md](RECOVERY.md).
Preserve the current working SD apps, resident fallback, keyed agent and private
host state before changing recovery behavior. `scripts/foston_preserve.py`
collects signed read-only file backups and verifies hashes; it does not create
a full firmware image or prove restoration. Keep an independent private copy.
Do not roll back host counters/device journals, synchronize resident flash,
flash firmware or inject boot failures as routine recovery testing.
