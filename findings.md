# Findings

## 2026-09-29 source audit
- Foston absent from current host USB inventory; no /dev/ttyUSB0. Hardware tests pending.
- Current terminal kills repllog.exe/rnaapp.exe on startup and each second, defeating sync.
- Current terminal only reads weather files and has fabricated default readings; no TCP receiver.
- FM code guesses IOCTLs, DLL signatures/units, and writes OEM driver registry settings.
- Gallery rediscovery resets selection, exposes only five folders, and decodes images without a decoded-memory limit.
- ArchMerOS cinema state is ~/.local/state/archmeros/cinema-mode.json. TURZX is stopped/restored via its system service.
- Battery is dead per user: software launch after CE boots and electrical power-on are distinct problems.
- Cached BlueZ identity 00:18:E4:1F:03:AF (GPS system) advertises Audio Sink, AVRCP, handsfree; no SPP/PAN in its cached service list. Connection timed out, so playback is unverified.
- Live weather fetch for the existing configured city succeeded (Farroupilha, RS); host bridge is enabled and Linger=yes.
- Root transport service is not installed. sudo -n requires a password; no privileged action was taken.
- Native POSIX shim tests exercise actual shared CE packet parsing and disconnect recovery. Physical WinCE rendering, audio, and power behavior still require the device.

- Firefox MPRIS currently supplies title, artist, play state, position, and duration correctly.
- USB `045e:00ce` binds to Linux `ipaq` as `/dev/ttyUSB0`.
- Opening `/dev/ttyUSB0` is not an ActiveSync session; the current host log reports only a successful file descriptor open.
- `mero-media-ctrl.exe` currently kills ActiveSync processes and probes guessed COM ports, preventing the documented PPP path.
- The repository already has the required `CLIENT`/`CLIENTSERVER` + `pppd` launcher and documents custom TCP over PPP.
- Simultaneous Linux/WinCE FAT access cannot be made reliably coherent with `os.sync`, rename, or read-cache flush guesses.
- The image/gallery visualizer does not use ActiveSync networking. It reads `\SDMMC\Stream` files prepared by `scripts/mero_stream_bridge.py`.
- Live PPP failed before IP negotiation: `chat` sent `CLIENT`, waited for `CLIENTSERVER`, and timed out on every attempt.
- SynCE 0.15.2 `synce-serial-chat.c` proves the correct direction: read six-byte `CLIENT` from the device, then write twelve-byte `CLIENTSERVER` from the host. The repo's PPP script had this reversed.
- Corrected PPP negotiated `192.168.55.101`/`192.168.55.100`, but firewalld blocked TCP port 5000 until `ppp0` was assigned to the trusted zone.

2026-10-03 laptop comparison: SSH cris@192.168.1.80 now works using dedicated mero_foston_laptop key. Laptop cris-MS-1454 runs 7.0.0-28-generic and exposes EHCI root hubs, whereas mero-machine 7.1.8-arch1-3 external ports use xHCI. Laptop old_scheme_first=N, use_both_schemes=Y, autosuspend=2 versus mero-machine -1. Laptop successfully enumerated 045e:ffff as high-speed on usb2-1.4 at 23:32, then disconnected 23:37 and repeatedly failed as full-speed with -32. Thus older EHCI is not immune; xHCI-only explanation is insufficient. Mount reported unclean FAT and partition truncated beyond EOD. No repairs performed. Update bundle copied/extracted under laptop /tmp, not yet deployed to absent device.

2026-10-03 mounted follow-up: laptop Foston on EHCI usb2-1.6, 045e:ffff, SD UUID6232-3562 /media/cris/6232-3562. Small shell executable read succeeded. Device power/control=on, runtime_status=active excludes autosuspend for this session. Export disk=15522816 sectors; MBR partition start8192, recorded size15515648, expected end15523840: beyond exported disk by1024 sectors/512KiB. Linux trims exposed partition to15514624. Prior read at sector15522688 failed and USB disconnected. Copied existing system_dump/launch_strings into /tmp/foston-laptop-evidence-20261003; window_dump capture missing. Safely unmounted with udisksctl. Pending apps remain staged laptop /tmp; no deployment or filesystem repair on unstable transport. Direct SD reader recommended to verify real capacity/FAT and deploy without WinCE USB bridge.
