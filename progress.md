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

2026-11-05 agent feature: implemented per-device keyed build, HMAC-SHA256 command/reply protocol, monotonic durable job directories, cached results, bounded SD writes, staged SHA-verified installation with backups and rollback, read/list and named Mero process controls. Host CLI serializes callers and preserves timed-out pending jobs. Cross-language C/Python crypto and protocol checks plus pending/stale-result tests pass. Bootstrap published mero-agent.exe for a one-time download; user asked to run it. Live authenticated status/file tests pending. AGENTS.md records operating contract, original RTK/cloud SSH rules and safety boundaries. Key/header/keyed binary are private and excluded from Git.

2026-11-05 AGENT VERIFIED: authenticated hardware status/list; remote file write/read/update/rollback with exact bytes; probe launch/log/stop(1); large ARM upload. Fixed oversized outbound truncation via256-byte socket writes/512-byte OBEX PUTs, cancelling only pending read15. Added hash-bound safe agent upgrade and upgraded live agent at same navigation path (SHA40f81...30c3), keeping old binary. Large9,728-byte round trip and4,218-byte log pull passed. Remotely deployed/read-back shell0.2.13, backup ref26, SHA43dc...9ca; launched it, user confirmed usable, agent still answered in background. Host FTP restart/reconnection status30 passed. Private bootstrap executable removed from share, no pending job remains, key/header remain private. Firmware, resident flash and boot configuration untouched. Power-loss recovery is not hardware-tested.

2026-11-05 FINAL AGENT ACCEPTANCE: user confirmed remotely deployed shell visible and usable. Fresh authenticated status30 passed after host FTP restart; agent remains active in background. make, make check (companion, C/Python auth/path/transport/pending/cancel tests, OBEX codec), Python syntax and diff whitespace checks passed. Upgrade helper recovery branches now report rollback failure accurately; helper build checked. Key and keyed artifacts remain private; no pending job or public keyed bootstrap remains. Native firmware/boot/resident flash untouched. No Git commit/push requested for this new agent feature yet.

### Useful companions — 2026-11-06
- Added signed Bluetooth live snapshots and an in-RAM app mailbox; status job 40 verified increasing generations, age 2s, live weather and music.
- Deployed/read-back verified the shell, weather terminal, music deck, gallery, tuner, camera, CMD, probe, Bluetooth client and read-only screenshot helper. Original app/config backups remain in agent job receipts.
- Captured actual 480x272 weather and music screens. Weather showed current Farroupilha conditions and all three forecast days. Music showed Southside at 3:36 / 4:39, paused.
- Native Play/Pause button -> RAM event -> signed Bluetooth upload -> PC MPRIS worked in both directions; bridge restart did not replay the acknowledged press. Test left playback paused; later user presses remain under user control.
- Hardware standby test exposed gray OEM STATIC/SS_BLACKRECT rendering. Replaced it with a custom window that explicitly paints black; regression capture and disconnect-wake check are in progress.
- Artwork now uses a 112x112 RGB565 single-entry RAM cache, verifies SHA256 before display, purges obsolete covers and clears buffers on close. PC conversion and native purge tests pass; final device capture is pending.
- Final artwork capture (job 170) shows the matching Southside cover and real 3:59 / 4:39 timing. Native RAM-cache invalidation tests confirm previous pixels are zeroed on new-track receipt.
- Black-screen regression now passes: native capture is entirely black; the agent stays reachable. After stopped updates the screen recovers and clearly marks the cached track offline. Services are active again.
- Native default startup and close-to-shell captures passed (jobs 160/163); the temporary preference was rolled back and verified byte for byte (jobs 164/165). A physical cold-power check was requested separately.
- Final agent hash: 0587e2b61f42a5d48e86cd68ebbd10b2e52d2d0e649f5811e4dfad8e900807b6. Status job 176 reports age 2s, live weather/music, 37 MB free RAM and unchanged resident-shell Launch50.
- make, make check and diff whitespace checks passed. Seven hardware PNGs and a SHA256 manifest are retained under ~/.local/state/mero/foston-evidence/20261106/.
- Session stopped at the user's request after cold-start recovery. The user
  reported the default Music app working after vendor USB changes/reset;
  authenticated status then confirmed fresh Bluetooth telemetry. A native
  read-only report confirmed Serial_Class / COM6 and the OEM default query.
- Installed agent was subsequently upgraded to
  888efc5f3b6c7229cfaf0a1cb750efb37f646d56317a32891e1ce418481762c9,
  including separate keyed-agent identity and initialization/reconnect support.
- Shell 0.3.1, verified OEM USB control and the final explicit-pause cancellation
  fix are built/tested but NOT deployed. No new USB mode writes were made by
  this implementation. Detailed installed/candidate hashes and resume steps are
  in docs/NEXT_SESSION.md. No device job was pending at shutdown.
