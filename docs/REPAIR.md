# Foston repair and live sync

## Status, 2026-11-05

USB on mero-machine still fails descriptor negotiation before a storage device appears. Bluetooth is paired and connected. The OEM File Manager has opened the PC OBEX FTP service and the user successfully downloaded the proof file after switching the device from Mass Storage to ActiveSync/PC Link. Mass Storage had prevented WinCE from writing its own SD download destination despite no host mount. The user downloaded and executed native mero-bt.exe by setting the navigation path to the downloaded file. They confirmed CONNECT/LIST, proof download, and UPLOAD TEST all succeeded. The PC independently received the exact 42-byte expected upload. Shell 0.2.12 is built and shared, with a client fallback at SDMMC/BT; its updated launcher has not yet been physically tested. ResidentFlash and boot configuration remain untouched by current work.

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
