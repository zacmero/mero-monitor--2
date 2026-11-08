# Foston repair and live sync

## Status, 2026-11-05

USB on mero-machine still fails descriptor negotiation before a storage device appears. Bluetooth is paired and connected. The OEM File Manager has opened the PC OBEX FTP service and the user successfully downloaded the proof file after switching the device from Mass Storage to ActiveSync/PC Link. Mass Storage had prevented WinCE from writing its own SD download destination despite no host mount. The user downloaded and executed native mero-bt.exe by setting the navigation path to the downloaded file. They confirmed CONNECT/LIST, proof download, and UPLOAD TEST all succeeded. The PC independently received the exact 42-byte expected upload. The signed agent workflow now runs in the background. Shell 0.2.13 was deployed and read back byte for byte over Bluetooth, then launched; the user confirmed it is visible and usable. ResidentFlash and boot configuration remain untouched by current work.

The current source differed from the old handoff: its terminal had no TCP receiver and terminated `repllog.exe`/`rnaapp.exe` every second. Those processes own ActiveSync. The bridge also depended on a media player to service its connection. Both problems are fixed.

## Runtime behavior

- The obsolete music card wording is replaced by the sensor terminal: clock, weather, GPS, and FM remain. Only the two obsolete music desktop shortcuts are removed. Other apps and factory files are preserved.
- Host weather for the configured city is broadcast every half second as a cached snapshot; the provider is fetched every ten minutes. A failed fetch does not reset freshness. Startup failures retry after one minute. The city remains configured in `~/.config/mero/weather.conf` (`city=Farroupilha, RS` on this host).
- Weather includes coordinates and all three forecast days. The terminal labels offline or old weather as cached and shows a waiting state before any data arrives. It no longer starts with invented weather values.
- GPS accepts checksum-valid NMEA sentences, supports GP/GN talkers, and clears stale satellite targets. The radar sweep remains animated indoors; satellites are no longer fabricated.
- The FM page offers mute, tone, noise, and ambient chord modes. Waveform and energy bars derive from generated PCM. Audio runs only on the FM page. The first-boot default is mute; existing saved modes are preserved.
- Unverified FM IOCTLs, DLL calls, and OEM registry writes are disabled, with their source retained for investigation. Frequency buttons store a requested value; they do not prove a transmission. Set actual FM routing/frequency through the factory panel until its driver contract is verified. The app does not record incoming Bluetooth audio, so its scope cannot claim to measure that audio.
- Shell startup and the flash forwarder no longer overwrite the internal recovery binaries automatically. The forwarder waits up to five seconds for SD and can fall back to the vendor launcher or WinCE desktop if custom shells are absent. The explicit boot-target menu is still available; it is not changed automatically.

## Gallery

Folder discovery preserves the selected path. The folder picker pages through the existing limit of sixteen folders, five per page. Files are sorted by filename. Folder selection, fit, orientation, and interval persist.

`AUTO` fills photos whose oriented aspect ratio is close to the screen; extreme ratios are contained so portraits and documents are not heavily cropped. `FIT` and `FILL` remain selectable. Photos pan gently at four updates per second using one retained decode; paused slides stop moving. Stream folders default to one-second refresh with motion off; ordinary folders default to five seconds with motion on.

Each selected folder can contain `mero-gallery.cfg`:

```ini
fit=2
orient=3
interval=5
motion=1
```

Fit is `0=contain`, `1=cover`, `2=auto`; orientation is `0/1/2/3 = 0/90/180/270 degrees`. Touch controls save this profile. Image files are never modified. Decode limits are 8 MB compressed, 3 million pixels, and available-memory headroom. Oversized images show an error and can be skipped.

Gallery and webcam image content still comes from device storage. This repair adds sync standby to those apps, not a network image-transfer protocol. Do not write live frames through a simultaneously mounted FAT volume. A bounded JPEG channel over PPP is the next step for actual live webcam/gallery input.

## Cinema mode and startup

The bridge reads the existing ArchMerOS state file at `${XDG_STATE_HOME:-~/.local/state}/archmeros/cinema-mode.json`; no ArchMerOS edits are required. While the file exists, it sends `MERO:POWER:standby=1`. On restoration it sends `standby=0`.

Shell, gallery, sensor terminal, tuner, and webcam use a black fullscreen window and pause their work while keeping USB networking alive. This is software standby: it does **not** promise that the panel backlight powers off or that WinCE enters system suspend. Hard suspend could remove the USB wake path. If host messages disappear for fifteen seconds, the display returns to the local interface. Diagnostic command/probe tools do not implement cinema blanking.

The host user bridge is already enabled at login. The persistent PPP service below is prepared but not installed: this session has no noninteractive sudo access.

```bash
rtk sudo bash scripts/install_sync_service.sh
rtk systemctl status mero-sync.service
rtk systemctl --user status mero-bridge.service
```

The new PPP runner accepts only an `ipaq` serial endpoint belonging to USB `045e:00ce`, reconnects after unplug/replug, and uses the device's `CLIENT` / host's `CLIENTSERVER` handshake. It permits TCP port 5000 from the device with a temporary, narrow firewall rule. It does not enable Internet forwarding, NAT, or a trusted firewall zone. Stop any older PPP owner before installing. The existing Internet PPP script is retained for explicit Internet-sharing use.

The dead battery means electrical power-on and application autostart are separate tests. Software cannot guarantee that this Foston powers itself on when USB power returns. No boot registry change is justified until power-return behavior and the existing boot target are read on the device. If the power controller requires a button press, a hardware repair or supported power-on circuit is needed; there is no verified software workaround yet.

## Build, deploy, and rollback

```bash
rtk make
rtk make check
```

No RAPI tools are installed on this host. Deploying executables therefore requires the SD card to be accessible exclusively to Linux, either in mass-storage mode or a card reader. Exit the custom apps before changing USB mode. Leave sync mode for runtime updates.

```bash
rtk bash scripts/deploy_sd.sh
```

Deployment requires mounted filesystem UUID `6232-3562`. It validates ARM PE headers, stages and hashes all binaries, and saves prior binaries under `MERO/deploy-backups/<timestamp>` before replacing apps. It preserves configuration, factory navigation files, internal flash, and the boot registry. It does not provide an all-files transaction across a sudden power loss; keep USB power connected through deployment and host unmount.

To roll back, exit the apps, access SD exclusively, and copy the backed-up executable files into `MERO`, then flush and unmount. Do not select a new persistent boot target during initial verification.

Hardware acceptance checks still required:

1. Launch the new shell and return to the factory interface.
2. Open gallery, choose a folder on page two, change settings, restart, and check persistence.
3. Test a portrait, wide image, corrupt file, and oversized file without a crash.
4. Verify current weather over PPP without any music player running.
5. Toggle cinema while each supported app is open; restore it and unplug during standby to verify the fifteen-second recovery.
6. Listen to ambient audio; confirm mute on leaving the FM page. Verify actual FM through the factory controls.
7. Shut down the host, remove/restore USB power, and record whether the Foston powers on and which launcher starts.

## Useful next features, ranked

1. **Bluetooth audio destination:** the host's cached BlueZ entry `00:18:E4:1F:03:AF` (`GPS system`) advertises Audio Sink, AVRCP, and handsfree profiles. Try desktop audio, spoken notifications, or a focus timer through the device. Audio/FM routing and actual playback remain unverified; the current connection attempt timed out.
2. **Live host status over USB:** reuse the repaired TCP path for render/build completion, desktop recording status, timers, CPU temperature, or a compact task queue. This does not depend on GPS reception or Bluetooth serial support.
3. **Bounded JPEG snapshots over USB:** send downscaled images to gallery/cam without mass-storage access, with strict length limits and last-good-frame retention. Folder profiles can determine whether the display behaves as a photo frame, camera monitor, or document viewer.
4. **Verified backlight control:** capture the factory panel's actual power calls, then implement screen-off standby if USB survives and wake is reliable.

The cached Bluetooth services do not advertise Serial Port or PAN. That does not prove the hardware lacks them; it means generic Bluetooth telemetry is currently unsupported by the observed service list. Driver names alone are insufficient to assume a working profile. The web search tool failed during this investigation; Bluetooth conclusions above are based on local BlueZ output and repository dumps.

## Bluetooth shell menu

Shell 0.2.7: System Tools → Bluetooth provides radio on (connectable), discoverable, off, refresh, and pairing/devices. Mode changes require both optional bthutil exports and a successful status read; failed or missing APIs report unsupported without registry writes. Pairing opens an existing PhoneLink window or installed Windows PhoneLink.exe directly. This is the OEM pairing panel, not a newly implemented pairing protocol; availability and operation require a device test. The full vendor launcher is not opened. Display Tuner remains in Companion Labs. Killing vendor watchdogs now preserves PhoneLink so it cannot inadvertently break Bluetooth.

Deployment keeps prior SD binaries under MERO/deploy-backups and leaves the existing ResidentFlash recovery shell intact. Since that recovery shell has not been overwritten, launch the new SD shell explicitly after exiting USB mass storage to see this menu.

## Verified Bluetooth session (September 29, 2026)

While powered from the wall, GPS system (00:18:E4:1F:03:AF) connected and resolved services: SDP 1000, Audio Source 110a, AVRCP Target 110c, Handsfree 111e. This live list supersedes the earlier cached Audio Sink assumptions. AVRCP MediaControl1 Connected became true after explicitly connecting profile 110c. Selecting the available PipeWire headset-head-unit profile produced bluez_output.00_18_E4_1F_03_AF.1, mono 8 kHz CVSD. A quiet 0.5-second tone sent specifically to that output was heard by the user. The normal Steinberg PC output remained the default. This verifies audio, not filesystem access or a working custom-shell radio interface. No file-transfer/OBEX, PAN, or serial profile was advertised in this resolved session.

Device test of shell 0.2.7 confirmed bthutil control unavailable and Windows PhoneLink.exe missing. Shell 0.2.8 now builds locally with Export OEM Details, which inventories Windows files, reads only HciExt/BtA2dpSnd driver values, and tries to capture Launch.exe, btpower.exe, btdrt.dll and bthutil.dll into SDMMC/MERO/bluetooth-inspect. It executes no captured OEM initializer and writes no registry values. ROM copy failure is recorded; repeated exports preserve existing captures. This is diagnostic preparation, not a completed OEM Bluetooth control implementation. It has not been deployed. USB mass storage is needed to deploy it and retrieve its capture.

USB follow-up: repeated USB 2-3 full-speed descriptor/address failures (-71) on xhci_hcd, before a class driver can bind. Both EHCI PCI controllers are already present/loaded, but active external USB ports tested route to xHCI. Mass-storage/ipaq driver replacement cannot repair pre-enumeration failure. usbcore old_scheme_first=N, use_both_schemes=Y, autosuspend=-1; root is unavailable without interactive sudo, so no controller reset, port routing, module unload or sysfs changes were performed. A temporary old_scheme_first=Y trial is possible, with no guarantee; existing fallback already tries both schemes. User reports vendor Bluetooth File Manager folder /SDMMC/BT/, so OBEX/file transfer must not be ruled out solely from the audio-mode service list. Direct SDP probe failed with Host is down while device was trying USB, leaving that hypothesis unresolved.

2026-10-04 OEM capture: Windows inventory confirms bthpnl.cpl, ctlpnl.exe, BTDRT.dll and hciext.dll; bthutil.dll and PhoneLink.exe absent. Captured btpower.exe strings/imports identify BthReadScanEnableMask, BthWriteScanEnableMask and BthGetHardwareStatus from BTDRT.dll, plus RF test routines: never treat this executable as a safe generic radio power toggle. Launch.exe points to ResidentFlash2/App/jwdmain.exe. Shell 0.2.9 pairing action now invokes ctlpnl.exe with the confirmed Windows/bthpnl.cpl applet; physical behavior remains to be tested. Standalone mode API controls remain unsupported pending verified lower-level API signatures. Export CopyFile reported error5 despite partial files existing; captured executable completeness is unverified and they are used only for static investigation.

## Native Bluetooth file-transfer milestone (2026-10-04)

Captured ResidentFlash2/App/BT includes PhoneLink.exe, AnwManagerU.dll, ANWMOBILEU.dll, BlueDrv.dll, ObexGen.dll, BTControl.dll and BTService.dll. Capture log reports full byte counts/error0 for core app and transfer modules; Windows ROM DLL streams remained unreadable. Evidence retained in ~/.local/state/mero/foston-oem/20261004 with SHA256 manifest. PhoneLink imports/loads explicit FTP get/list/put operations. This supports implementing an outgoing FTP client, not assuming an incoming OBEX server exists on the device.

Shell 0.2.11 adds Bluetooth Files (mero-bt.exe). The native app starts the confirmed OEM PhoneLink backend in the background and requests radio power through its registered message. Reverse evidence: PhoneLink registered-message map associates WM_ANWBT_INMSG_BTPOWER with VA0x28b54; that handler branches on WPARAM and calls the OEM power setter with1/on or0/off. It creates no boot/ROM registry changes. AF_BT32, RFCOMM3, 40-byte sockaddr layout/address offset8/service GUID offset16 were checked against BTService.dll socket/connect calls. App connects to previously paired mero-machine MAC8C:86:DD:83:27:70 using FTP service GUID1106, then performs OBEX target handshake, root listing, ASCII basename downloads, and a fixed upload proof. Downloads are uniquely named under SDMMC/BT/Downloads; incomplete files are retained; installed apps are never automatically replaced. Scope is an initial flat-directory transfer test, not arbitrary folder browsing, Unicode names, or new-device pairing. Connection/transfer errors go to SDMMC/MERO/mero-bt.log.

Mero-machine now runs user mero-bt-ftp.service with a dedicated ~/.local/share/mero-foston-bluetooth root. It advertises UUID1106. BlueZ5.87 obexd could not register transport without a local desktop session because of its logind policy; tested BlueZ5.72 obexd copied from laptop runs as a private user runtime with its dependencies under ~/.local/share/mero/obex-compat. System packages and Bluetooth kernel drivers unchanged. Service enabled for user boot, with existing linger setting. Runtime source/unit is host/bridge/mero-bt-ftp.service; private binary runtime is not committed. Stop with systemctl --user stop mero-bt-ftp.service.

ARM builds and make check pass, including C checks against actual OBEX codec for target/ID/Unicode framing and malformed headers. Native Bluetooth handshake, download and upload still require physical verification. Laptop SSH became unreachable (192.168.1.80 neighbour FAILED), so shell0.2.11/mero-bt.exe are built in /tmp/mero-foston-bt-update.tar.gz but have NOT been deployed. Earlier shell0.2.10 remains on the card. Once installed, open Bluetooth Files, CONNECT/LIST, download bt-transfer-proof.txt, then UPLOAD TEST; verify downloaded/uploaded bytes before claiming transfer works.

## USB-free bootstrap verified with OEM client (2026-11-05)

The device initiated OBEX FTP to mero-machine. D-Bus server session confirmed Source8C:86:DD:83:27:70, Destination00:18:E4:1F:03:AF, FTP targetF9EC7BC4-953C-11D2-984E-525400DC9E09 and root ~/.local/share/mero-foston-bluetooth. User saw the actual shared executable names. Initial attempts to save both proof and native client reported read-only memory. Switching USB mode from Mass Storage to ActiveSync/PC Link, while retaining power, allowed the proof download. This is the first user-confirmed Bluetooth file download; on-device proof contents/checksum were not yet checked. Native app launch and upload test are pending.

Keep the device OUT of Mass Storage during Bluetooth downloads: WinCE must own the SD filesystem. The PC does not need USB enumeration or a mount for Bluetooth transfer. Use the native client once to download mero-bt.exe into SDMMC/BT, close the native FTP connection, then launch the downloaded executable through WinCE Explorer. Avoid the old mero-cmd Desktop/Kill UI actions: that old implementation still kills PhoneLink and mirrors binaries into ResidentFlash. The shell Advanced/WinCE Explorer action launches Explorer directly. The custom Bluetooth client stages uniquely named downloads in SDMMC/BT/Downloads and does not install them automatically.

## Verified custom-client transfer result (2026-11-05)

User confirmed that the downloaded mero-bt.exe executed through the navigation path and that CONNECT/LIST, proof download and UPLOAD TEST all succeeded. PC received foston-bt-check.txt containing exactly the expected 42 bytes: MERO FOSTON BLUETOOTH FILE TRANSFER TEST followed by CRLF. SHA256 d9db6b662e742c86f31bc2a5a1d3fa590b6cad73ebd966f4c3646c4957aa61ce. A live server FTP session was present. This verifies the custom client end to end, with the upload payload checked byte for byte; the downloaded proof contents were user-confirmed as successfully saved but not independently hashed on the device.

The USB-free bootstrap is now demonstrated: native File Manager downloads our executable, the navigation path launches it, and our app performs transfers using the existing OEM backend without returning to the vendor UI for each operation. Pairing remains native; arbitrary nested browsing and non-ASCII filenames are not implemented. Shell0.2.12 can launch the client directly from its initial BT download directory, preserving the installed Mero files and resident-flash recovery shell.

## Agent-enabled device (2026-11-05)

The repository now includes AGENTS.md and scripts/foston_agent.py. A keyed mero-agent.exe polls signed jobs over the verified outbound FTP link, preserves a durable sequence and cached receipts, and uploads authenticated results. The host serializes callers and verifies reply identity, operation, size and SHA256. Timeouts remain pending until wait resolves them; cancel-read is available only for nonmutating reads. Jobs are retired after acknowledgement and payloads archived outside the public share. Keys and generated build headers are local/private and ignored by Git.

Implemented commands: status, ls, get, put, rollback, launch, stop, wait, cancel-read, bootstrap and upgrade. Reads cover SD/Windows/resident storage; writes are bounded to SD Mero/BT/Photos/Stream. App controls are limited to named Mero executables and protect the agent process. Ordinary writes cannot replace the running agent. Upgrade uses a separate helper bound to a complete plan hash, checks old/new executable hashes, preserves a backup and navigation path, restores on launch failure and verifies the reported running executable hash. It changes no ROM/boot registry settings.

Live verification: authenticated status reported38MiB free RAM and6890MiB free SD initially. Jobs3–8 wrote a30-byte test file, read it back exactly, replaced it, then rolled it back and verified original bytes. Jobs9–13 backed up probe.txt, launched mero-probe.exe, retrieved its refreshed log and stopped exactly one probe process; agent stayed available. Job14 uploaded a9,728-byte ARM probe copy with validated PE/hash. The initial multipart return failed with truncated OBEX frames and the read-only job15 was explicitly cancelled; no mutation was replayed. Bounded256-byte writes and512-byte PUT PDUs fixed the path.

The agent upgraded itself at SDMMC/BT/Downloads/101907-mero-agent.exe; running SHA25640f81babffdc16d5f609ac0e1cf5cf0e5d781226a9ba3531f78bf137605830c3 matched the candidate. Old agent preserved at SDMMC/BT/AgentUpdate/old-4329b0ea8b6554d7.exe. Job23 returned the probe copy with exact byte equality. Job24 retrieved a4,218-byte log. Job26 installed shell0.2.13 (58,368bytes), with its previous version preserved under agent job26. Job27 read it back and matched SHA25643dc068fefafb01ae36fe9908cd1230e3797a4e13bfe9bf864c6b226cc9e79ca. Job28 launched it; job29 proved background agent availability, and the user confirmed the shell is usable. Job30 authenticated successfully after restarting the host FTP service, confirming reconnect behavior.

No forced power-loss test was performed. Recovery journals/backups are implemented, but hardware power interruption remains untested. Agent status can verify its own path/hash. Keep agent mode on and the device in ActiveSync/PC Link. After power loss, launch the configured navigation app if the existing boot setup does not do so automatically. Firmware, boot state and resident-flash recovery are outside this agent interface.
# Live companions: hardware verification, 2026-11-06

Shell 0.3.0 groups the app catalogue under **01 – Companion Labs** and provides
an explicit **02 – Default App** picker. Weather, music, gallery and Frequency
Matrix are on the first app page. Optional old visualizer/terminal entries are
shown only when their executables exist. Existing app files and gallery settings
were preserved.

The host bridge now supplies weather, YouTube Music and cinema state through
signed Bluetooth snapshots. The agent publishes telemetry in shared RAM; it does
not create SD backups for each update. Weather animates locally and shows fetch
age, forecast and offline/cache state. Music uses actual MPRIS timing and accepts
controls only for an exact `music.youtube.com` session.

Music artwork is one 112×112 RGB565 image in RAM, verified against the signed
SHA256. The display buffer is reused. Obsolete artwork is cleared when new track
metadata arrives; buffers are cleared when the app closes. There are no new SD
artwork files. PC-side thumbnail caches are bounded to four files.

Verified on the device:

- All updated app binaries were backed up, transferred and read back byte for
  byte over Bluetooth. The current agent remains at the same navigation path.
- Screen captures show real Farroupilha weather and all three forecast days,
  and the correct Southside thumbnail, title, artist, 3:59 / 4:39 and paused state.
- The native Play/Pause button sent an authenticated event to the PC. Play and
  pause both worked. Restarting the PC bridge did not repeat the acknowledged
  toggle. Later user interaction remains under user control.
- A temporary saved Weather default opened after a fresh shell countdown.
  Closing it returned home and did not relaunch it. The previous preference was
  restored byte for byte through the agent rollback operation.
- A signed standby snapshot produced an all-black native screen capture while
  the Bluetooth agent continued answering. With updates stopped, the screen
  recovered and showed the cached/offline deck. Services were restored afterward.

The standby test exposed an OEM quirk: `STATIC` with `SS_BLACKRECT` painted gray.
The shared client now uses a window that explicitly paints black. This is
software blanking; it does not claim hardware suspend or backlight power-off.

Read-only boot inspection found Launch50 pointing at
`\ResidentFlash\MERO\mero-shell.exe`. The resident executable contains the SD
shell handoff path. The updated SD shell starts the existing provisioned agent
from `SDMMC/MERO/agent.cfg` in the background. No boot-registry values or resident
recovery binaries were written. A physical cold-power test remains separate from
the verified shell restart/default-app test.

Hardware PNGs and their SHA256 manifest are stored outside Git at
`~/.local/state/mero/foston-evidence/20261106/`. Reproduce a capture with the
read-only `mero-screen.exe` helper and `scripts/foston_screen.py`.

Ideas retained for later work: [Companion roadmap](COMPANION_ROADMAP.md).

### 2026-11-06 — Ambient Home deployed and verified

Added native Ambient Home with authenticated PC-folder selection, previous/next, pause/resume, weather fetch-age and stale labels, locally animated weather icons/motion, and shell exit. Host resizes JPEG/PNG/WebP/BMP asynchronously to 240x136 RGB565; portraits occupy the safe area clear of weather/chrome. No photo library is written to device storage. Separate bounded RAM mapping is hash/size verified and gated by foreground demand. Host retains only two owned resized frames and waits for actual display acknowledgement before advancing. Existing Local Gallery, user images, other apps and saved Music default are preserved.

Hardware evidence: keyed agent upgrade status 254, hash edf15ccb370da4fcde25100b9a78786dc484285738e8b8e8c4448dac8b9b131d (backup BT/AgentUpdate/old-5e5ab33eea18d983.exe); final Ambient put/backup 291 and byte-exact get 293, 32,256 bytes, SHA 8256a604e767a54dc5c1e7dd5665e2c46a74b6c9ae90bf6b82644c2e3dc18560. Shell 0.3.2 put/backup 273 and byte-exact get 294, 63,488 bytes, SHA a242e974d9034f4de3128a448d0dd9ec76e7f8ba77e1e4df1f438b201f82d160. Backup 258 preserves previous 0.3.1.

Actual UI automation jobs 275–289 verified folder menu/selection (Screenshots), stable pause for 25 seconds, next while paused, resume, restoration to Pictures and return to shell. The first test caught a tap discarded behind a pending heartbeat; a single expiring UI retry fixes that race and has actual-C contention/expiry/session tests. Final screen jobs 295/296 show streamed Pictures, animated sun, 26 C and FETCHED 1m. Shell screenshot shows 0.3.2 with YouTube Music still the saved default. Final status 297: live age 3s, weather/music reception, 38 MB free RAM, Launch50 unchanged. No unresolved job.

Full make/check and diff whitespace checks pass. New tests cover real RGB565/portrait rendering, source preservation, bounded host cache, stale folder worker results, signed/deduplicated/expired controls, display backpressure, actual native frame hash/length/demand/close guards and tap retry. Launcher tests now use the actual app enum and verify Ambient ID 15 persistence without changing legacy IDs. Cold-power startup and cinema standby of this new app were not physically retested; shared code is reused. Usage: docs/AMBIENT_HOME.md. Current work remains uncommitted.

## Orientation, cinema backlight and compressed photos — verified

Installed shell 0.3.3: 4814b4cfbecc4ffbc7357a833d283c02933d7cec426166ace43e79266b6cbc9d, put/backup 365, exact readback 366. Ambient: 70b4e8bb6c1d2c83816ed05edb346bad885c36db53469d347221765cf14e08db, put/backup 368, exact readback 369, relaunched 370. Agent: be6a08442d84d28a402b876043beabdc25dbb6b3d2b5ed6eccccbfdc465e2648, verified upgrade 343, backup BT/AgentUpdate/old-49f9d693b0628339.exe.

Shell main 06 – ORIENTATION and Ambient menu ROTATE both toggle the saved PC portrait setting. The whole Ambient interface rotates, including touch mapping, weather, photo and controls; other legacy apps keep their layouts. Capture 350/351 verified rotated rendering; native main-shell tap 360 changed the saved choice back to landscape. Clean view contains no orientation button. Left running in landscape after tests.

JPEG transport is deployed: PC sends bounded compressed thumbnails using the existing content-addressed filenames; hash verified before JPEG decoding, strict 240x136 dimensions and existing RGB565 RAM mapping. Legacy raw frames remain accepted. Bundled decoder tests cover valid JPEG, truncated headers and invalid dimensions. Socket bounds remain 256-byte writes and 512-byte PUT PDUs. Status 326 measured raw 65,280 bytes / 7,806 ms GET / 11 ms decode; status 331 measured JPEG 10,046 bytes / 1,330 ms GET / 21 ms decode. These are different frames, not a controlled identical-image benchmark.

Backlight evidence: active Drivers/BuiltIn/BKL, name BKL1:, GetDevicePower result 0/state D0. Standard SetDevicePower D4 and restore D0 both returned success and reported matching states. The agent now follows cinema state with reversible BKL1 power-off, keeping CPU and Bluetooth awake. Original state is journaled in SDMMC/MERO/cinema-backlight.state before dimming and cleared by a zero record after restore; preserve this recovery journal. No guessed IOCTL, brightness registry keys, firmware or boot changes. Whole-device suspend/wake is unverified and not used.

Full cinema integration jobs 352–357 verified all-black captured pixels, BKL state D4 during cinema, D0 after marker removal, and uninterrupted authenticated Bluetooth operations. Temporary test cinema marker was removed. Evidence is private at ~/.local/state/mero/foston-evidence/20261106-orientation-cinema/. Full make/check and diff-check passed. Recovery-journal behavior after actual power loss has not been physically tested. Work remains uncommitted.

## 2026-11-07 — physical backlight correction, shortcut and shell fixes

The user confirmed that BKL1 power D4 did NOT extinguish the physical LCD glow. Retract earlier physical-off claims: only returned power state and black pixels were verified. Reverse engineered the exact OEM Gui.dll (SHA dd68fde7d8ac585f96410706fad39709d014bd16498073c4a75629a1506bfccb), SetBackLight at VA 100145c8 calling 1001a440 and GetBackLight at 1000ac30 calling 1001a394. BKL1-only GET 0x22000c, DWORD output; SET 0x220008, DWORD input; vendor level mapping raw=14+5*level (0..10). Native driver reports zero bytes returned while writing a valid raw value; OEM checks BOOL, not byte count. Actual read/set/restore observed 19→14→19. User still reports unchanged physical glow. Physical dimming/off remains unresolved; do not claim it is fixed from readbacks. Driver registration is Drivers/BuiltIn/BKL, DLL buses.dll. Supported get of Windows/buses.dll failed (job 422). No guessed controls, raw GPIO, ROM dumping or CPU suspend were attempted.

Current agent 8934fcbdd1a1a208ac931b3c4b12510e81cb5acae86a492aa96c42ea457b7e2d, upgrade status 404; backup BT/AgentUpdate/old-1f46728bedebf6da.exe. It uses OEM minimum raw14 on cinema and restores the original raw value with a version-2 recovery journal. check_backlight.c tests actual code for zero-count reads, exact device/control/buffer bounds, save failure, invalid brightness and restoration after agent restart.

Root cause of shortcut mismatch: Super+Alt+S invokes archmeros-side-standby.sh→side-dpms.sh, NOT cinema-mode.sh. Bridge now follows DP-2 DPMS false as well as cinema-mode.json. Exact side script off/on integration verified black pixels and raw14 during standby, raw19 after restore. This is not proof of physical backlight change. Host monitor state was restored after testing.

Shell now rotates its entire layout and touch coordinates too (the previous toggle only controlled Ambient). Physical portrait shell screenshot and transformed touch were checked. Portrait explicitly saved as default at user request, with PC config backup preserved. Shell 0.3.5 colors: amber/coral/mint/violet/cyan/rose, tinted item panels, thicker highlighted default. Ambient controls also colored; selected folder amber. Shell put/backup407, exact readback408, SHA afc78e8902c1ba0428b997f937bacc3fe120878029bb1c5bb48b66f9d8151f71. Ambient put/backup410, exact readback411, SHA 74e4516b7809db7ba6160f3256024094205e60e143e7065c77e7c0eb1fa59380. Colors/portrait screenshot423/424. Left on portrait shell. Full builds/checks passed; work uncommitted.

Pending user clarification: whether the native vendor brightness slider ever physically changes this screen; do not require leaving our shell just to answer from memory. Docker is only the ARM compilation environment on the PC, not the device runtime; apps run natively on WinCE.

## Direct manual minimum script — 2026-11-07

User requested the equivalent of lowering the native settings slider over Bluetooth, as a simple script. Added scripts/foston_backlight_min.py and native wince/probe/mero-backlight-min.c. Optional build target `make -C wince/probe mero-backlight-min.exe`. Installed helper put/backup427, 6144 bytes, SHA e1d1655f559c3198e25f43b86a2a3e2932a3dfebcb99ede9f43800261983b0c0. First actual script run launched a new helper and retrieved the process-bound report: pid2410375522, tick26750948, ok1, before19/request14/after14. Unlike earlier probes this action leaves brightness at minimum; it does not restore after eight seconds. Physical dimming still needs user confirmation; an async question is pending. Do not claim physical backlight off from this readback. Host stale/failed reply checks pass. Device remains at raw14 after this user-authorized manual action. No reboot or vendor UI was required.

## Local Backlight control app — installed 2026-11-07

User requested a touch app to manually compare the native brightness range without vendor UI. Added wince/backlight/mero-backlight.c, `rtk make backlight`, and first-page Companion Labs entry Backlight (stable startup ID16; previous IDs unchanged). Buttons: DIMMER, BRIGHTER, MINIMUM, MAXIMUM, RESTORE (level at entry), SHELL (keeps selected level). Driver-reported level 0–10 and constant white/gray patches help visual comparison. Local controls work without Bluetooth once installed, follow portrait/landscape state and honor cinema blanking. No firmware, resident launcher, navigation path or startup preference changes.

Native app put/backup432, exact readback438: 14,848 bytes, SHA05d1c80ae2af0cef23360cc4a930d52f038e833a2a0d980bb539dfd9cf93563a. Shell0.3.6 put/backup435, exact readback439: 66,048 bytes, SHA17936f2198acb04a2f729fbb03c2880b0a40fb6016d2e77e154dd523388714f3. Actual app screen436/437 showed all six readable controls, patches and LEVEL0/10 in portrait. Left Backlight open for user testing. Physical minimum-vs-maximum difference remains user-observed; readback does not prove emitted brightness.

Root make, make check and diff-check passed. check_backlight_app.py compiles the actual C button actions and tests increments, both clamps, min/max, restore and unavailable/failed driver handling. Usage docs/BACKLIGHT_APP.md. Existing tuner and other apps preserved. Current work uncommitted; no pending job at completion.

## User-confirmed brightness and power-return behavior — 2026-11-07

User confirmed the Backlight app works and that its minimum is physically still bright. This supersedes earlier uncertainty about whether the OEM brightness range has any visible effect; the remaining unsupported feature is full physical backlight-off while keeping Bluetooth awake.

User also explicitly confirmed: battery is dead; external power is required; reconnecting power enters charging mode only, and pressing the physical power button is required to start normally. Thus automatic Windows CE/shell/app startup on power return is NOT verified or working. Application autostart after CE boot is a separate, working feature. Charger-mode behavior points to the startup/power firmware path, but its exact implementation has not been established. No known safe software-only charging-mode-to-CE boot control was found. Do not change boot registry/bootloader or flash firmware under the app-work authorization. Any firmware setting or hardware power-button pulse solution requires separate evidence-backed investigation.
# Latest evidence — 2026-11-07

Ambient full-screen cropping and weather tap-to-hide/restore are installed and screen-capture verified (deployment442/readback444). Status461 confirms the keyed Bluetooth agent remains connected. Initial read-only firmware/backup audit: [FIRMWARE_RESEARCH.md](FIRMWARE_RESEARCH.md). Existing OEM file copies and the recovered factory default registry hive are partial backups, not a complete firmware image or verified restore route. No firmware, boot policy or power-state change was made during this investigation. The user confirmed native minimum brightness works but remains bright; physical full backlight-off and cold power-return auto-boot remain unresolved.

Mero Terminal milestone: keyed agent upgrade521 and app529/readback530 verified. PC command round trip, native touch pwd and managed-session persistence worked. Seamless physical/touch mode test532 observed0→1→0 with both CASUE keyboard handles open in mero-bridge and no desktop forwarding window. Permanent scoped udev permission rule installed/user-confirmed; active local seat uaccess and ACLs verified. New-machine setup is in INSTALLATION.md; current hashes and evidence supersede older entries in NEXT_SESSION.md. No firmware/boot writes were used.

Overnight terminal checkpoint supersedes those early builds: agent595/status623 SHA9b3488c3aa3cf4a0ca852eb750567f8b390774b823a5ab366514cd07e295a0df, app597/readback598 SHA18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60. Functional terminal and duplicate-input guards are committed in66a5290; probe in0d694fb. **Latency remains unresolved**: isolated deferred report619 measured6314ms and one output. Native queue→PC event arrival is the leading unresolved leg; PC published output162ms after event receipt. Temp FlushFileBuffers measured0ms. Full evidence and next tests are at the top of NEXT_SESSION.md. Physical keyboard capture is off, final status623 was healthy and no pending job remains. User stopped work for sleep; do not continue hardware experiments tonight.

## 2026-11-07 resumed: portrait YouTube deck
Music now follows the same authenticated AMB portrait flag as the shell. Portrait uses a 272x480 logical layout, rotates the full interface, and transforms shell/playback/seek touch input back to logical coordinates. There is no extra per-app orientation button. Single-entry 112x112 RGB565 RAM artwork lifetime remains unchanged.

Initial portrait layout was inspected through screen jobs629/630 (no active PC music session). Final tested helper build installed with stop641/put642/get643; size179936, SHA256 `8fe7fdca283886e6821bb62a71c097fba6e598450c4552441c05f99f28f791d8`, backup642 (previous portrait build); original pre-portrait rollback backup626. Native orientation parsing and inverse touch tests pass.

### Terminal transport checkpoint
Protected agent update status638 verified SHA25600580b7a3e7ae1c935da9081330b93dfd9f9c3dca0c96342bea3f144e50bd1a0; previous agent preserved at BT/AgentUpdate/old-b2bf6449c1d858a2.exe. Only small control PDUs changed to256, bulk512/socket256 retained. First-send queue/write/transfer metrics now exposed. Probe records foreground failures explicitly.

Fresh isolated launch646/649/652 reports received authenticated terminal output in2103/941/1330ms (median1330), exactly one output each. Last observed host event-to-ACK publication154ms. Status654: queue109/write1/firstsend263ms, liveGET359ms, period503ms, age0. Still variable; no claim of instant responsiveness or controlled before/after speedup. No persistent boot, USB, firmware or keyboard permission changes. See NEXT_SESSION for exact receipts/hashes and remaining latency work.

## Latest music update: edge-to-edge cover (2026-11-07)
Portrait artwork now fills a272x272 square across both screen edges. Track text, progress and controls sit below it; landscape and the single112x112 RGB565 RAM source are preserved. Stop658/put659/get660 verified180638B SHA256 `93f73cf9da319731d1203485905f4112ce31d569a4818cfc5106bee3f3f1f973`; backup659 restores the prior portrait deck. Launch661 and screen662/get663 verified actual playing artwork (Up on the Hill, Fun Lovin’ Criminals), full-width cover and unobstructed controls. ScreenSHA256 `8e0384b11e19d49a97cdc70598d22222bdd8b07af153347e22e5cd24ebc6c35b`. Full make/check and whitespace checks passed.

## Current artwork checkpoint: native272x272 (2026-11-07)

The prior112-square notes below are historical. Firefox's current MPRIS export was only60x60. The host now prefers matching high-resolution song artwork, prepares272x272 JPEG (quality92, lower only to enforce60000B cap), and retains the restricted Firefox file as offline fallback. Current cover is19288B, smaller than the old25088B raw transfer. Host owned caches remain bounded; no cover files are written to the Foston.

- Agent authenticates encoded hash then validates exact272x272 JPEG dimensions before allocation/decoding. One147968B RGB565 cover in versioned `MERO_ART565_V2`; music drawing cache is bounded to the same image. Old artwork clears on track changes/app close. Ambient still requires240x136. Socket/PDU limits unchanged.
- Protected agent upgrade through signed status671: SHA256 `5d1997b73b34fdde90ff4048755479d2f894129e3606fad8ae6f29299a454744`; previous agent backup `\SDMMC\BT\AgentUpdate\old-52462ed6200a1cda.exe` (old agent uses112-square V1). Navigation path/key unchanged.
- Music stop664/put672/get673/launch674:180638B SHA256 `f3b83d8469aaf1cea8c1c2bd253def9083f34b314302c0da780b6968435c173d`, previous binary backup672 (112-square ABI). Rollback must restore matching agent/music/host formats together; a code-only rollback is insufficient.
- Bridge restarted with new JPEG publisher, both services active. Actual screenshot675/get676 visually confirms sharp full-width album cover and working track/timing feed (Korean Bodega / Fun Lovin’ Criminals). Capture SHA256 `9a7e142ec015b48854ae513f479f3c1f709747cff6a66dba901db8b8235d3111`.
- Full make/check and whitespace checks pass. Tests cover high-resolution preference, undersized network rejection, source preservation, offline file fallback, reused export path, stale track handling and actual native decoder dimensions/truncation/encoded-size bounds. AGENTS artwork contract now reflects272-square single-cache limit authorized by the user's resolution request.

---

## Terminal latency: staging failure and repair

Fresh isolated input-to-frame baselines were 6092 and 6854 ms. A compact-only feed reduced transfer size to about 850 bytes but still measured 6591 ms. Status695 showed a failed first attempt before any OBEX packet. The local-retry build measured 1713 ms; status706 identified ERROR_DISK_FULL (112) on the CE temporary-file staging write despite ample free SD space. This error path unnecessarily closed a healthy Bluetooth connection.

The agent now sends transient signed input and codec messages directly from RAM through the existing bounded sender. Agent/deployment journals and durable keyboard delivery intent keep their original flush rules. The first two RAM-only samples were 928 and 798 ms with exactly one output each; status717 confirmed successful first upload, two packets, reply160, zero staging time/error. Foreground terminal frames omit unrelated weather/music/gallery metadata and fall back to the generic signed feed if missing or stalled.

Final agent: protected upgrade through status713, SHA256 `095a52e746a8f0b21214bda20297bacacfd76121d09c0131c920bdf40fc9ccb0`; previous intermediate agent backup `\SDMMC\BT\AgentUpdate\old-cf5f69f1bfe802ab.exe`. The pre-session high-resolution-art agent is preserved at `BT/AgentUpdate/old-c97b39c17c98f9cc.exe`. Host/key/navigation path unchanged; source transport milestone2486955 pushed.

Final terminal candidate: stop728/put729/get730, 32256 bytes, SHA256 `8b6328097055822b3ba907fbb4223b502699fa9c481ec3bfa132aaa9794f1d10`, backup729 preserves the intermediate timer-only app; backup721 preserves the pre-session app. It coalesces an input-message batch into one posted flush, keeping foreground and one-command guards. Final repeated measurements and compatibility evidence follow in NEXT_SESSION.

The UI timer and eager/coalesced flush experiments did not improve measured latency and were not retained. Signed rollback745 restored the original terminal; get746 verified31744B SHA256 `18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60`. Source/tests match the pre-session terminal implementation. Only transport/compact-feed changes remain installed. Experimental backups are preserved.

## Keyboard-first terminal shortcut

Terminal put756/get757 verified31744B SHA256 `00da812c39768bf87468d97b4eeaf661daf146ab6b4c6d592a896a08f7c1691d`; previous app preserved as backup756. Launch758 and screenshot759/get760 verified full terminal/SHOW KB with touch keys hidden after physical mode exit. Agent remains095a52e7….

Super+Alt+Insert is installed in active ArchMerOS Lua and legacy configs, with a windowless wrapper. Hyprland reload has no config errors and exposes exactly one Insert/modmask72 binding. Activation launches the named terminal through the signed agent, waits for released keys/fresh foreground confirmation, then captures. Captured evdev detects the same chord to release. Native PC OFF leaves touch keys hidden; SHOW KB/HIDE KB is manual. Existing keyboard permissions and emergency Ctrl+Alt+Escape remain.

Fast25ms host polling modestly reduced sampled deliveryACK median572→451ms; sampled PC-published output median959→932ms shows no major end-to-end gain. These measurements do not end at device paint. Full checks pass; actual physical shortcut feedback is pending. No boot, firmware, USB, key or agent changes.

## Ctrl+Space control-byte correction

Ctrl+Space was rejected as NUL by native keyboard validation, preventing Herdr's configured detach prefix and potentially timing out PC capture. Touch CTRL+Space also lacked the NUL mapping. Both paths are corrected: NUL delivery is restricted to foreground Mero Terminal; all other targets reject a NUL-containing packet before partial posting. Durable journal/dedup/foreground and malformed UTF-8 checks remain.

Agent upgrade771 verified SHA2561b857bc49f90f037b35976ba0a8b09a55636d2885abf3d69a58e55a578727010, prior backup BT/AgentUpdate/old-c105d08e76025a12.exe. Terminal put772/get773 verified31744B SHAb3e2ad0879f44d992b0ec080aab5dbacdf8e0501cc9c0e8f1ce58abda60198e0; backup772 preserves prior app. Full checks pass. User confirmed the updated sequence returned to Bash. Herdr sessions were not stopped.


### Firmware mapping pass — 2026-11-07
Read-only probe deployed with backup 778; read-back SHA256
`1aceed706720a869c2e504695036d7cc884d05ac01b2a969927fa4b2a548845f`.
Fresh report GET 781 completed healthy/untruncated: MTK/APOLLO, 134 registry
keys, 35 BuiltIn driver entries, 92 loaded modules, 551180 bytes object-store
free. Factory hive decoded offline (1084 sections), original hash unchanged.
See docs/FIRMWARE_RESEARCH.md for evidence, private paths and capability limits.
No boot/flash/power-state/registry changes. Full firmware backup and tested
restore route still missing. Next: offline OEM contracts and correctly typed
read-only store/partition geometry; do not infer privilege from exported APIs.
