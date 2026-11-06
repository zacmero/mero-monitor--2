# Progress

## 2026-09-29 repair
Inspected shell, gallery, terminal, bridge, PPP launcher, host service, and ArchMerOS cinema script. No device writes performed. Building changes against the installed CE Docker toolchain; live deployment depends on reconnecting Foston.

Completed source repairs and built shell, gallery, media, tuner, cam, and flash ARM executables. Root make succeeds. Offline checks cover TCP fanout/reconnect, cinema state, complete weather snapshots, stale-cache failures, exact USB identity, ARM PE validation, actual CE fragmented packet parsing, disconnect wake recovery, and DWORD wrap-around. Python/shell syntax and systemd unit validation pass. Existing third-party STB and shell warnings remain.

Initial weather-injection check exposed newline-delimited snapshot corruption; sanitization now happens before formatting. Initial native test had a wrong literal write length; corrected to include its final newline. An apply_patch context miss was resolved without changing unrelated files. Web search returned a decoding error, so Bluetooth findings use local evidence only.

Restarted the already-enabled host bridge; its live provider fetch succeeded. No Foston USB enumeration and Bluetooth page timeout prevent deployment and physical checks. No ROM, boot registry, internal flash, or factory-file write was performed on the device. Prepared backed-up SD deployment and a root PPP service; administrator authentication is unavailable in this session.

- Inspected live USB, mounts, service logs, MPRIS state, host bridge, WinCE receiver, and PPP launcher.
- Confirmed the existing WinCE source builds, while the live transport remains unverified and nonfunctional.
- Selected PPP/TCP using the existing line protocol as the minimum reliable design.
- Replaced host raw serial sends with a reconnecting TCP listener on `192.168.55.101:5000`.
- Added a nonblocking WinSock client and switched the WinCE polling timer to it.
- Cross-built the 73 KB WinCE executable and confirmed the deployed SD copy has the same SHA-256 hash.
- Python syntax, host TCP loopback exchange, build, and whitespace checks passed.
- Started transient root service `mero-ppp.service`; it is waiting for `/dev/ttyUSB0` while the device remains in Mass Storage mode.
- PPP handshake attempts failed before creating `ppp0`; no TCP connection was possible.
- Changed storage sync to fixed-file overwrite/open/read behavior and made `[LIVE SYNC]` require changing file content within three seconds.
- Removed storage freshness from the live badge and corrected the PPP chat sequence to expect `CLIENT` then send `CLIENTSERVER` without a trailing carriage return.
- Verified `ppp0`, bidirectional ping, bridge listener, and an established WinCE TCP client at `192.168.55.100:1034 -> 192.168.55.101:5000`.

2026-09-29: Added shell 0.2.7 Bluetooth menu with optional mode API controls and direct installed pairing panel; watchdog preserves PhoneLink. Shell ARM build and make check passed. Deployed SD apps with verified backups at MERO/deploy-backups/20260929-224416-38512; ResidentFlash and boot registry untouched. Bluetooth hardware behavior pending physical test.

Bluetooth live session: audio and AVRCP connected while on wall power. User heard host test tone over HFP; default desktop output unchanged. Confirmed previous custom menu unsupported on OEM image. Shell 0.2.8 read-only OEM export built; check suite and diff checks passed, not deployed because device is currently off USB. Native OEM controller remains pending evidence.

2026-10-04: User explicitly requested deployment despite unstable USB. Mounted via laptop user-session GIO, deployed 8 apps to SD UUID6232-3562 through laptop; staging and backups verified by deploy_sd.py, then all 8 installed hashes independently matched staged sources. Backup MERO/deploy-backups/20261004-000008-6794. ResidentFlash, boot registry, factory files and configs preserved. Updated SD shell 0.2.8 includes diagnostic OEM Bluetooth export, not a verified OEM radio implementation.

2026-10-04: Native WinCE bthpnl applet reports controller-not-present10050 while OEM scan works. Goal is actual Bluetooth transfer/custom controller, not vendor UI workaround. Deployed shell0.2.10 with expanded manual ReadFile/WriteFile vendor capture (ResidentFlash2/ResidentFlash exe+DLL catalog and core BT DLLs, processes;32MiB/free-space bounds; no OEM commands, registry mutations or ROM writes). Stage/backups passed; shell installed hash matched. Backup SD MERO/deploy-backups/20261004-013701-5489. Await user capture then retrieve to trace OEM transport/profiles. Built-in controller implementation remains unresolved.

2026-10-04 continued: Created native WinCE OBEX FTP client and shell0.2.11 integration, verified OEM power message handler/static BT socket ABI, started/enabled headless user FTP service with UUID1106 and isolated staging root. Preserved OEM capture and manifest. ARM builds, existing companion suite, OBEX wire codec tests and diff checks passed. Device deployment pending laptop network recovery; no hardware file-transfer success claimed.

2026-11-05 resume: Foston USB still fails descriptor/address negotiation on mero-machine xHCI (-71), while Bluetooth is paired/trusted/connected. Existing mero-bt-ftp user service is active after reboot. Verified live org.bluez.obex/server/session1 with Destination00:18:E4:1F:03:AF, Source8C:86:DD:83:27:70, FTP targetF9EC7BC4-953C-11D2-984E-525400DC9E09 and dedicated PC root. User can see app executable names in native File Manager; this is consistent with verified server folder session. Asked user to download proof.txt and native mero-bt.exe as USB-free bootstrap. No file download success yet verified, native client not deployed. Shared mero-bt.exe matches ARM source build, size82470 SHA25671c8e22c01f277f87109e5257662ef9144b9d9c8eea68b06232dd49c0f4d2cd3.

2026-11-05 breakthrough: user reports proof-file download SUCCESS after changing Foston USB from Mass Storage to ActiveSync/PC Link. Bluetooth FTP endpoint and shared root verified by live D-Bus session. Asked user to download native mero-bt.exe, close native FTP connection, launch via WinCE Explorer and report CONNECT/LIST status. Native client transfer test still pending.

2026-11-05: User reports native mero-bt.exe downloaded over Bluetooth and executed successfully by setting navigation path to the downloaded executable. This verifies USB-free native-app bootstrap. No current FTP session or test upload on PC at subsequent check; asked user to tap CONNECT/LIST, download proof and UPLOAD TEST to verify native client. No boot target or ResidentFlash files changed by agent.

2026-11-05 VERIFIED MILESTONE: user confirmed all native mero-bt operations successful: CONNECT/LIST, proof download, UPLOAD TEST. Host independently verified exact42-byte upload and SHA256d9db6b662e742c86f31bc2a5a1d3fa590b6cad73ebd966f4c3646c4957aa61ce; FTP session4 active. User requested commit and push. Updating docs and validating complete source repair/Bluetooth milestone; OEM captures/private runtime/built binaries excluded.
