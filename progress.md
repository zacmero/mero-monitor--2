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

### Resumed hardware deployment
- User reported unattended startup into Music with Bluetooth and correct track data. Status job 195 confirmed live telemetry age 2s.
- Agent upgrade verified SHA256 22c494eba344ecef15939e2af2a1cdff2bccd6c3f2aa4da5c905afec41100422; previous agent preserved at BT/AgentUpdate/old-5a9c8be03aff0293.exe.
- Shell 0.3.1 installed with backup job 203 and byte-exact retrieval. Status 206 remained live, age 2s, after startup ActiveSync preparation. Saved app preference and resident boot configuration preserved.
- Shell Connect / Agent user acceptance and another post-update power-cycle test remain pending.

- Fixed agent-mode file UI: permanent worker busy no longer disables manual controls. List/download/upload requests are consumed by the agent worker, preserving serialized OBEX access. Connect queues a listing, live heartbeat replaces stale reconnect text, and buffered painting prevents partial controls.
- Installed agent SHA256 68bb414de26889d65b26e179b9ab601c602eeb8d9a72b7e1bf129c0224b06c85. Actual UI capture shows file rows and all six controls. Native upload test delivered exact 42-byte proof (SHA256 d9db6b662e742c86f31bc2a5a1d3fa590b6cad73ebd966f4c3646c4957aa61ce) while agent remained ON. No pending job afterward. Focused queue check, full make check and ARM build passed.

### 2026-11-06 — Ambient Home priority and weather verification

User deprioritized extra PC controllers and agent dashboards; retained as future possibilities in the roadmap. Gallery inspection confirmed local device folders remain the source: PC-folder streaming is still required, with bounded RAM frames and no photo-library deployment. Agent status job 243 reported live age 2 seconds and weather reception. Host bridge logs show successful ten-minute refreshes and 28-to-26 C change. Launched weather (244), captured screen (245), retrieved and hash-verified capture (246): actual UI shows 26 C, forecasts, BLUETOOTH / LIVE, FETCHED 6m AGO. No gallery streaming implementation or deployment claimed.

### 2026-11-06 — Ambient Home deployed and verified

Added native Ambient Home with authenticated PC-folder selection, previous/next, pause/resume, weather fetch-age and stale labels, locally animated weather icons/motion, and shell exit. Host resizes JPEG/PNG/WebP/BMP asynchronously to 240x136 RGB565; portraits occupy the safe area clear of weather/chrome. No photo library is written to device storage. Separate bounded RAM mapping is hash/size verified and gated by foreground demand. Host retains only two owned resized frames and waits for actual display acknowledgement before advancing. Existing Local Gallery, user images, other apps and saved Music default are preserved.

Hardware evidence: keyed agent upgrade status 254, hash edf15ccb370da4fcde25100b9a78786dc484285738e8b8e8c4448dac8b9b131d (backup BT/AgentUpdate/old-5e5ab33eea18d983.exe); final Ambient put/backup 291 and byte-exact get 293, 32,256 bytes, SHA 8256a604e767a54dc5c1e7dd5665e2c46a74b6c9ae90bf6b82644c2e3dc18560. Shell 0.3.2 put/backup 273 and byte-exact get 294, 63,488 bytes, SHA a242e974d9034f4de3128a448d0dd9ec76e7f8ba77e1e4df1f438b201f82d160. Backup 258 preserves previous 0.3.1.

Actual UI automation jobs 275–289 verified folder menu/selection (Screenshots), stable pause for 25 seconds, next while paused, resume, restoration to Pictures and return to shell. The first test caught a tap discarded behind a pending heartbeat; a single expiring UI retry fixes that race and has actual-C contention/expiry/session tests. Final screen jobs 295/296 show streamed Pictures, animated sun, 26 C and FETCHED 1m. Shell screenshot shows 0.3.2 with YouTube Music still the saved default. Final status 297: live age 3s, weather/music reception, 38 MB free RAM, Launch50 unchanged. No unresolved job.

Full make/check and diff whitespace checks pass. New tests cover real RGB565/portrait rendering, source preservation, bounded host cache, stale folder worker results, signed/deduplicated/expired controls, display backpressure, actual native frame hash/length/demand/close guards and tap retry. Launcher tests now use the actual app enum and verify Ambient ID 15 persistence without changing legacy IDs. Cold-power startup and cinema standby of this new app were not physically retested; shared code is reused. Usage: docs/AMBIENT_HOME.md. Current work remains uncommitted.

## Ambient clean view follow-up

Removed loading captions and folder/filename strip. Full-screen photo plus weather is now default; tap to open menu, FULL SCREEN to close. Retain last verified photo during transfer. App put/backup 300: e9fbf6536318c6ee0a84db76348905cac0effbd17c174d867416a27373ff7b3f, 31,744 bytes. Capture 302/303 verified weather-only clean startup without chrome. Reduced native agent polling sleep from 2s to 0.5s and host live publish interval from 2s to 1s; no packet/write bounds changed. JPEG transfer remains unimplemented and no throughput improvement is claimed.

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

## Wallpaper corrections — 2026-11-07

PC frame generation now center-crops every photo to fill the full viewport, including portrait; no small foreground/blurred canvas. Weather widget taps hide it and taps on its same invisible hit area restore it without opening controls. Folder chooser suppresses this hit area. Actual-C hit tests and full-screen crop checks pass; full make check passed. Ambient put/backup442 and exact readback444: 0cc787f516124cb723bd1e0738d79eded2f4ccbd33d2014f301e871ddf26d980, 32,768 bytes. Native tap446 hid weather; capture447/448 showed edge-to-edge photo with no menu/widget. Tap449 restored weather. Portrait and selected Pictures source preserved. Continuing authorized read-only firmware investigation; no flash writes or boot changes.

## Initial firmware investigation and final connection check — 2026-11-07

Audited partial backups, independently verified all64 existing manifest entries, and preserved a hash-verified117-file private archive. Retrieved factory Windows/default.hv (job460); this and existing OEM copies are not a complete firmware/bootloader backup. Bounded native read-only probe v2 deployment456/report458 found live storage profiles, charger-wake policy and a distinct ScreenOff state. No power transition, boot setting, flash write or raw disk read was attempted. Details and next safe research steps: docs/FIRMWARE_RESEARCH.md.

Final status461: agent_ready, live age2 seconds, weather/music present, 37MB free RAM. Ambient correction checks passed again; full make check had passed after the new firmware helper checks. Ambient stays open in portrait with weather restored. No reboot or user action is needed for the wallpaper changes. Working tree remains uncommitted.

## Milestone publication — 2026-11-07

User requested committing and pushing all current milestones with comprehensive messages. App/control source and usage documentation committed as 0f86777; read-only firmware diagnostics and research report committed as d1080b5. This handoff commit records the complete hardware evidence, installed hashes, tests and unresolved limits. Full make and make check passed again, including optional firmware/minimum helper targets; diff checks passed. Private keyed builds, OEM binaries, hives and evidence archives remain outside Git. The push result is verified separately against origin/master.

## Mero Terminal implementation — 2026-11-07

Added a persistent managed tmux/Bash session, compact fastfetch, bounded text snapshots, native portrait/landscape app and touch keyboard. New startup ID17 preserves older IDs and optional terminal app. PC-keyboard input was verified through the signed PC→Foston→PC path (PC_KEYBOARD_OK); native portrait taps typed pwd and returned /home/zacmero. Initial native font names all fell back to proportional Tahoma; embedded licensed DejaVu bitmap now gives exact fixed cells. Bash pane PID1577082 survived bridge restarts.

User rejected the initial desktop forwarding window. Removed it: background evdev capture now runs inside mero-bridge, with the active Hyprland XKB layout, exact supported foreground custom-app delivery, bounded packets, durable signed acknowledgement/dedup and release on failures, lock/inactive seat or process exit. PC KEYS hides the digital keyboard; TOUCH restores it and releases the PC keyboard. A rapid-toggle test exposed delayed mode acknowledgements; added a local requested-mode state and actual-C regression coverage.

User completed the permanent scoped udev installer. Verified /etc/udev/rules.d/70-mero-foston-keyboard.rules, uaccess tags and active-user ACLs on both CASUE interfaces. No repeated sudo is required. Added docs/INSTALLATION.md for new hosts, custom keyboard IDs/stable paths, seat requirements and recovery boundaries; linked it from README and AGENTS. Initial temporary setfacl setup is superseded by the persistent rule.

Agent final upgrade status521: d94c8f18fbc851b5f22d236b0553e4bd53d8d0f77432bfb2fa81986838374866, backup BT/AgentUpdate/old-0996c52f2a19c7b2.exe. Boot Launch50 and existing startup preference were unchanged. App deployments used backups and exact readbacks. Full make check, native app builds, installer shell syntax and udev rule verification passed. Current changes are uncommitted.

Final verification: app529/readback530 is8ec22febaee2888c31f1c913d99d85ff4834195a015ad68b151ec94f5cd4c651 (30,720 bytes). Final background mode test532 observed0→1→0, actual event2/event5 handles open in the bridge, and no forwarding window. Screenshot533/get534 showed the touch keyboard restored with readable fixed-width text. Status535 confirmed agent_ready, live age1 second and39MB free. User requested matching their custom fastfetch; compact profile now displays ArchMerOS x86_64, user/host, kernel, Bash, Hyprland, uptime and RAM. Refreshed the existing session and preserved its unfinished input via the Readline kill ring. User requested permanent setup instructions for new machines; docs/INSTALLATION.md and AGENTS/README links are complete.

Final screen536/get537 (28,888-byte RLE, SHA6bd79cc3e827cc413c68a8031cce8893ea18e6d9b15df1bcbe541d04790096b7) was decoded and visually inspected: ArchMerOS x86_64 and all compact system fields fit in portrait above the touch keyboard. Latest full make check passed. Left touch mode active with no desktop forwarding window and the existing unfinished command preserved.

## Terminal duplicate-input and latency repair — in progress

User reported duplicate keys and very slow refresh. Physical CASUE interfaces both advertise all typing keys; selected one actual event source per engagement while retaining both grabs, rejecting duplicate DOWN/orphan-repeat events. Native touch now accepts one DOWN until UP/focus loss, captures release and highlights press. Moved input before frame capture, made music polling asynchronous, added demand-gated150ms host publication,50ms native bus consumption,25ms terminal agent pause and1s job polling. Added actual timing counters and a harmless marker probe. Baseline5367ms to received output frame; polling-only samples6085/2784ms, so no stable gain claimed. Added negotiated signed zlib snapshots using bundled stb, bounded decompression and checksum,8s compatibility lease. Full tests/build passed before upgrade567 (af0083df578512248981071b8204cd87c1fcbcd8d9442ad23456c4ee39d96afd; backup BT/AgentUpdate/old-8399a60b7dab1a95.exe). Measuring final compressed path and verifying repeated-touch suppression on hardware.

## Overnight stop requested by user — 2026-11-07

Stopped experiments and prepared commits/push/handoff. Latest isolated deferred/no-intervening-RPC sample619 still measured6314ms, queued137ms, ack5786ms and one output. Host event arrived16.996s after launch, output published17.158s; this points to native queue→PC event arrival, not slow PC rendering. The earlier launch-journaling explanation did not account for this isolated sample and is superseded. Temp/SD timing622 showed no slow FlushFileBuffers. Detailed evidence, untested256-byte control-PDU hypothesis, probe observer-effect possibility and exact next steps are at the top of docs/NEXT_SESSION.md.

Final installed agent9b3488c3aa3cf4a0ca852eb750567f8b390774b823a5ab366514cd07e295a0df; public terminal18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60. Final status623 healthy, fresh live feed,38MB RAM, physical keyboard capture off. The temporary pending.json seen while status623 was executing was its active read, not a stale job. No firmware/boot writes occurred. Git checkpoints alone do not undo installed builds; protected device backups and rollback procedures are in the handoff.

Publication: source/app checkpoint66a5290 and native diagnostic checkpoint0d694fb created with detailed messages. Root make and full make check passed again; staged checks caught and removed only trailing whitespace in the diagnostic client/license copy. Generated OEM/keyed binaries and credentials were excluded. After status623 retired, verified pending.json absent. Final handoff commit records unresolved latency and the user's request to continue meaningful commits/pushes tomorrow. Remote push is verified separately.

Benchmark audit: initial immediate-injection timing results were confounded by the launching agent job persisting its SD receipt and uploading its reply. Those provisional figures must not be presented as steady-state typing latency or a valid speedup comparison. The corrected helper defers injection10s after launch; its driver verifies RPC completion before8s and avoids other device jobs until the sample finishes. Final worker optimization uses signed foreground presence outside the input mailbox, pending-event upload before screen GET, and authenticated job-sequence hints with timed fallback. Agent final upgrade595 is9b3488c3aa3cf4a0ca852eb750567f8b390774b823a5ab366514cd07e295a0df (backup BT/AgentUpdate/old-7172786efe30cb64.exe); app597/readback598 is18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60,31,744 bytes.

### 2026-11-07 resumed device work
- Device authenticated at job624, fresh telemetry, existing agent9b3488c3… intact. Implemented portrait music deck following AMB portrait flag; retained bounded RAM artwork and shell-only orientation toggle. Native test covers orientation parsing and inverse touch coordinates.
- Initial music put626/get627 SHA77b16b44…; launch628, screenshot629/get630 verified portrait visually. Current PC music session unavailable, so this was layout verification. Extracted unchanged touch transform into tested helper afterward; final binary deployment is in progress (job642).
- Agent protected upgrade632–638 verified SHA00580b7a… and backup BT/AgentUpdate/old-b2bf6449c1d858a2.exe. Added first-event queue/write/send timings and capped only small control OBEX PDUs at256, retaining512 bulk/256 socket bounds. Actual sender harness passes reconstructed bytes/errors/bounds.
- First latency launch639 did not update report because upgrade foregrounded the agent. GET640 exactly matched yesterday's SHA2b1310fd…; rejected as stale, no performance claim. Probe now writes an explicit ok=0 report on foreground failure, rather than leaving an old success report.

- Final portrait music put642/get643 match179936B SHA8fe7fdca…; backup642 retains previous portrait binary, backup626 retains pre-portrait music. Full make and make check passed, plus new actual-sender test.

- Valid isolated probes646/649/652 after foregrounding terminal:2103,941,1330ms; one output each. GET647/650/653 authenticated fresh reports. Median1330; no controlled speedup claim. Host observer saw154ms arrival→ACK publication on last run. Status654 queue109/write1/firstsend263/liveGET359ms, live age0.
- Portrait code committed/pushed0e542bc; control transport committed454cfe4. Final Music launch655; retaining existing terminal Bash/session and physical keyboard capture off. Remaining terminal variable latency and human duplicate-input verification documented.

- Final music screenshot656/get657 matched the already inspected portrait image byte-exactly (SHA1677f905…); foreground Music, no PC playback session.

### Portrait music: edge-to-edge cover
- User requested larger song artwork reaching the screen edge. Draw the same bounded112x112 RAM source into a272x272 portrait square at x0,y36, with song text/progress/buttons below. Landscape retained. Portrait controls moved to y421 and touch mapping fixture updated.
- Full make/check and diff check passed. Stop658/put659 backed up the previous deck;180638B SHA25693f73cf9da319731d1203485905f4112ce31d569a4818cfc5106bee3f3f1f973. GET660 byte-exact read-back confirmed. Launch661, screenshot662/get663 visually verified actual playing cover (Up on the Hill) reaching both edges, with text and controls below.

### Higher-resolution music artwork
- Found source bottleneck: current Firefox-exported art is60x60. New host pipeline prefers matching high-resolution track art, resizes to native272x272 and sends JPEG bounded60000B (current19288B). Offline fallback remains restricted Firefox cache. Host caches remain bounded; device never stores cover files.
- New versioned272-square RGB565 RAM cache, exact-dimension JPEG decoder and hash validation; one cover, cleared on song/app changes. Source/decoder/tiny-image rejection/track-bound-cache tests passed, as did full make/check.
- Stop664; protected upgrade through status671 verified agent5d1997b73b34fdde90ff4048755479d2f894129e3606fad8ae6f29299a454744, backup BT/AgentUpdate/old-52462ed6200a1cda.exe. Bridge restarted successfully; both user services active. Matching app deployment underway.

- Final music put672/get673 verified180638B SHAf3b83d84…; backup672 preserves previous112-square app. Launch674 then screen675/get676 visually verified crisp native-width actual album artwork and fresh Korean Bodega track/timing. ScreenshotSHA9a7e142e…. Formats/versioned ABI and paired rollback requirements documented.
