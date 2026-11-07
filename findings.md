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

2026-11-05 agent workflow: org.bluez.obex.Client1 is available, but CreateSession(Target=ftp, Source8C:86:DD:83:27:70, Destination00:18:E4:1F:03:AF) failed Unable to find service record. Cached remote FTP UUID is insufficient evidence of an incoming server. Use the hardware-verified device-initiated FTP channel for command/result polling. Proposed per-device HMAC key, strictly increasing sequence, bounded SD write targets, staged/hash-verified writes, durable backups and rollback; no ROM/boot operations.

Agent hardware acceptance: HMAC status/list/write/read/update/rollback all passed. Native process launch/stop and log retrieval passed. Larger outgoing OBEX frames were truncated; 256-byte socket writes and 512-byte PUT PDU bound fixed binary round trips. Live agent hot-upgrade preserved navpath and verified new executable SHA. SD shell0.2.13 deployed/read back exactly and GUI confirmed usable; background agent status and host-service reconnection verified. No USB mount needed. Current backup for prior shell is agent job26; current agent backup BT/AgentUpdate/old-4329b0ea8b6554d7.exe. Keyed binaries/header and host state must not be public or committed.
# Useful companions investigation — 2026-11-06

- Firefox initially omitted timing while YouTube Music was loading. A later
  MPRIS GetAll snapshot reported a real 268-second track and 224-second position.
  The bridge now reads metadata/timing/capabilities together, uses the URL video
  ID as track identity and accepts only the exact music.youtube.com hostname.
- Ordinary YouTube ASMR was open in another Firefox instance. Generic media
  commands risk controlling it; it is excluded from targeting.
- Agent job 40 verified live Bluetooth generations, age 2s, weather and music
  present. The CE named shared memory APIs work on this hardware.
- Read-only Launch50 inspection returned ResidentFlash/MERO/mero-shell.exe.
  Downloaded resident shell SHA256 c8dde485eb8402c3ec3b2b525917a3fed4774116e4c8da031eb22dbcfa7183b9
  contains the SD shell handoff path. No boot-registry changes were made.
- Old shell treated autolaunch=0 as missing and could reload a registry default.
  Fixed with a found-config flag. Preference writes now stay on SD with staged
  save, backup and failed-rename recovery; old numeric IDs remain compatible.
- Old CMD Desktop/Hide UI could terminate PhoneLink/custom apps and silently
  copy binaries over resident recovery. Replaced the sweep with known OEM-window
  hiding; automatic recovery mirroring is removed. Explicit legacy boot commands
  remain outside normal app deployment.
- Cold-start regression: user reported shell 0.3.0 with Music still saved, no
  auto-launch, a brief BT connection/drop and Mass Storage. After vendor USB
  changes/reset the user reported normal operation; fresh signed status and a
  native read-only report confirmed Serial_Class / COM6. Exact causality remains
  unisolated; do not declare cold-power startup permanently fixed.
- The old shell USB toggle used an unverified 0x220008. Actual Setting.exe
  routines use a GUID-enumerated controller bus, query 0x220014 (756 bytes),
  default set 0x220018 and runtime change 0x220010 (256-byte names), with default
  rollback on failure. Native read-only query succeeded. Change/rollback branches
  are only offline-tested and await deployment/hardware validation.
- Separate keyed-agent window identity avoids confusing ordinary mero-bt.exe
  with the agent. The installed agent supports it, while the old shell still
  needs its staged Connect / Agent and startup ordering/retry update.

## Ambient Home transport and verification

240x136 RGB565 is exactly 65,280 bytes, within the existing 65,536-byte OBEX RAM buffer including its terminator. This permits a bounded verified frame mapping without adding a large-message transport or SD frame writes. Do not increase dimensions beyond that buffer without changing and testing the transport. The device acknowledges displayed frame generations; host slideshow timing starts then, avoiding skips on slow transfers. Folder actions share the signed event path, bound to a per-bridge-session folder token and 30-second event expiry. Native UI taps retry locally for at most 15 seconds when a heartbeat owns the single mailbox, preserving the token captured at tap time. Native acceptance verified this fix. Existing gallery stays available as Local Gallery; Ambient Home owns PC streaming. See NEXT_SESSION/REPAIR for installed hashes and job evidence.

## Wallpaper corrections — 2026-11-07

PC frame generation now center-crops every photo to fill the full viewport, including portrait; no small foreground/blurred canvas. Weather widget taps hide it and taps on its same invisible hit area restore it without opening controls. Folder chooser suppresses this hit area. Actual-C hit tests and full-screen crop checks pass; full make check passed. Ambient put/backup442 and exact readback444: 0cc787f516124cb723bd1e0738d79eded2f4ccbd33d2014f301e871ddf26d980, 32,768 bytes. Native tap446 hid weather; capture447/448 showed edge-to-edge photo with no menu/widget. Tap449 restored weather. Portrait and selected Pictures source preserved. Continuing authorized read-only firmware investigation; no flash writes or boot changes.

## Firmware backup audit, initial inventory

Repository backup/factory-reference is a minimal configuration/reference set, not an SD image: nine files totaling 72,306 bytes across backup. The preserved OEM capture under ~/.local/state/mero/foston-oem contains 117 files totaling 16,097,872 bytes. No .img/.nb0/.nbf/.rom/.bin firmware or bootloader image was found in those audited backup roots. Reverse-tools is tooling, not a firmware backup. sha256.json uses filename keys and is being independently verified, including duplicate-name checks. Do not call any of these a complete recovery backup.

Read-only firmware-probe first build caught a WinCE ANSI lstrcpyA linkage mismatch and nonportable 64-bit printf format. Replaced with bounded snprintf and explicit high/low DWORD volume sizes before deployment. No failed binary was installed.

Live firmware probe verified (451/452): CE5.0 build1400, report120 registry keys, healthy1/truncated0; launchPID matched retrieved report. HKLM init retains our Launch50 and standard device.exe/gwes.exe/services.exe/btpower.exe. StorageManager registry DECLARES MSFLASH BINFS, Hive Disk and ResidentFlash profiles; these are configuration profiles, not a measured physical partition map. Hidden/bootloader sectors are not backed up. Windows nk.exe and buses.dll have ROM-module attributes0x2047; default.hv0x47. MSFLASH uses block.dll and DSK prefix, index9.

Important power evidence: Drivers/BuiltIn/SPC uses spc.dll and already has ChargerInWakeup=1, ChargerOutWakeup=1, Suspend=1 and Unattended=1. Thus charger wake policy exists in the running OS; charging-only behavior cannot yet be pinned exclusively on the bootloader. These settings may describe resume rather than cold boot. No values were changed. ScreenOff power state leaves Default=D0 and requests D4 for the display power class, distinct from the failed BKL1 power-state test; it is only a candidate for a future timed reversible test, not yet executed.

The device BACKUP directory appears in root listing, but agent ls failed (453), which can happen for an empty directory; no emptiness/completeness claim yet. SD logo.nb0/logo.bin/bgrlogo.nb0 are each261120 bytes, equal to480*272*2; likely raw logos rather than OS images, pending header inspection. ResidentFlash2 ARC.BIN/BEE.BIN identities remain unknown.

Static OEM analysis: PowerDown.exe, jwdmain.exe and JWDConfigUpdate.exe are available offline. PowerDown uses Gui.dll CPowerThread; jwdmain refers to PowerDown and POWER_KEY_EXIT. JWDConfigUpdate refers to SysConfigInfo.ini and registry configuration; its filename alone does not establish a firmware flasher. None was executed. Gui.dll CBatCtrl::SetPWState at0x10001e7c is only a global value store, so it is not an API for forcing system power.

Microsoft CE5 GetDevicePower documentation states the default class is generic and the value can be cached; POWER_FORCE queries the driver. The new read-only probe uses that documented query, plus GetSystemPowerState. This does not change power state. ScreenOff remains a separate candidate and has not been invoked. Storage-manager enumeration needs the correct Storemgr.h/Storeapi ABI; those headers are absent in the installed build image, so no guessed store struct was deployed.

Follow-up probe457/458: 121 keys, complete/healthy/no truncation. Active storage paired by registry section: DSK3 is the high-capacity SD memory driver; DSK9 is BuiltIn/MSFLASH. The earlier line-filter printout mixed adjacent key/name pairs; only section-paired identities should be used. Current system power state is on. Cached and forced driver BKL queries both reported D0 in this current awake state. BKL is in the generic power-manageable class. BACKUP enumeration returned error18 (no more files) with no entries; no backup image was found there. ARC.BIN/BEE.BIN headers do not show the common PE/MSBIN signatures in their first32 bytes; identities remain unknown. logo.nb0 is261120 bytes and starts with zeros, consistent with a raw display logo but not proof of firmware content.

Created private OEM-file archive with all117 source files and independently compared each archived file hash to source. Compressed archive SHA4decc711641a788f03c7d0db30a5651a3cd28c5f55c607dcc0c12844f923aa7c, size6456471. Filename explicitly says NOT-full-firmware. This is another local file-level copy, not an independent external backup or a verified restore image.
