# ActiveSync media synchronization

## Current task: Foston repair (2026-09-29)
- [x] Inspect current source, USB detection, and ArchMerOS cinema behavior.
- [x] Restore safe live sync independent of obsolete music playback; integrate cinema standby.
- [x] Fix gallery folder selection, paging, persistence, and adaptive presentation.
- [x] Repair terminal weather and PCM visualization; preserve GPS and FM features safely.
- [x] Build ARM binaries, run focused checks, document recovery and useful follow-ups.
- [ ] Deploy and test on hardware when USB is visible. Currently no 045e device or ttyUSB port.

Transport installation also awaits administrator access (sudo -n requires a password).
Host bridge is enabled with user lingering and running the updated code. Device electrical power-on and actual panel standby remain unverified. See docs/REPAIR.md for the physical acceptance checks.

Prior plan below is historical; its claimed TCP receiver no longer exists in current terminal source.

## Goal
Stream YouTube Music state from the Linux bridge to `mero-media-ctrl.exe` reliably over ActiveSync PPP/TCP, using Mass Storage only to deploy the built executable.

## Phases
- [x] Inspect current MPRIS, raw serial, PPP, and WinCE polling paths.
- [x] Replace host raw serial writes with a reconnecting TCP server.
- [x] Replace the active WinCE COM polling path with a reconnecting Winsock client.
- [x] Document the PPP startup and deployment sequence.
- [x] Build and run focused host checks; deploy when Mass Storage appears.
- [x] Verify the live device connection after the user switches USB back to ActiveSync and launches the updated app.
- [x] Verify corrected SynCE handshake, PPP address creation, TCP connection, and live metadata.

## Decisions
- Preserve the existing `MERO:NOW:` and `MERO:WX:` newline protocol.
- Host listens on PPP address `192.168.55.101:5000`; WinCE connects as a client.
- Do not send live state through the shared FAT volume.
- Do not terminate ActiveSync processes; they own the PPP transport.
- Follow SynCE's protocol exactly: read `CLIENT` from the device, then reply with the 12-byte `CLIENTSERVER` string before PPP negotiation.
- Assign transient `ppp0` to firewalld's trusted zone so the WinCE client can reach host TCP port 5000.

## Errors Encountered
| Error | Attempt | Resolution |
|---|---:|---|
| Web search tool failed to decode responses during diagnosis | 1 | Used repository documentation and live hardware state. |
| First dead-code removal patch did not match the previously reformatted C source | 1 | Left the unreachable legacy function in place; the active timer path uses only TCP. |

### 2026-11-05 Bluetooth milestone
- [x] Bootstrap native executable over Bluetooth without USB mount.
- [x] Run native client through navigation path.
- [x] Verify custom client list and download with user.
- [x] Verify upload byte for byte on PC.
- Publishing authorized by user; commit and push outcome recorded in Git history.

### Agent-enabled device workflow (2026-11-05)
- [x] Inspect existing native transport and PC OBEX APIs.
- [x] Confirm direct PC-initiated FTP unavailable (Unable to find service record).
- [x] Add authenticated command polling to native transfer app.
- [x] Add host CLI with verified results, backups, rollback and app controls.
- [x] Document agent operating contract in AGENTS.md.
- [x] Validate protocol and file-operation safety; bootstrap device build.
- [x] Verify live agent file update/read/rollback over Bluetooth.

### Useful apps and coherent shell (2026-11-05)
- [x] Inspect current media/weather, launcher/defaults and close behavior.
- [x] Record all brainstorm ideas and priorities.
- [x] Add live Bluetooth weather/state feed without per-snapshot job backups.
- [x] Improve current weather view with local animation and freshness.
- [x] Centralize apps in Companion Labs; persisted default app and consistent return to shell.
- [x] Verify YouTube Music state/control capabilities and implement a reliable deck if supported.
- [x] Build/test, deploy over agent with backups and verify on hardware.

- [x] Add current-track artwork with a single RAM cache and immediate obsolete-cover purge.
- [x] Finish thumbnail capture, black-screen/wake regression and startup/return hardware checks.
- [ ] Physical cold-power startup and automatic Bluetooth reconnection: awaiting user test.

### Cold-start regression — active investigation
- [x] Recover agent after user power cycle; pending read job 177 resolved with wait.
- [x] Implement startup USB mode using verified OEM behavior; read-only native query and offline control/rollback tests pass.
- [x] Prepare shell agent start/reconnect controls and delayed/retried startup.
- [ ] Re-test saved YouTube Music default and Bluetooth after real power cycle.
- User confirmed v0.3.0 and saved Music preference, but no app start/BT persistence; USB booted Mass Storage. Shell BT button reports launch failure. Exact keyed agent recovery path has been provided.
- User's vendor USB changes/reset restored operation; native report confirmed Serial_Class / COM6.
- [x] Shell and agent fixes deployed, now superseded by 0.3.2. Hardware mode change from Mass Storage and cold-power retest remain pending.
- Paused overnight at the user's request. See docs/NEXT_SESSION.md for exact installed and prepared builds.

## Ambient Home implementation — 2026-11-06

1. [x] Add bounded asynchronous host photo-folder renderer and authenticated ambient actions.
2. [x] Receive verified 240x136 RGB565 frames into a separate RAM mapping, gated by the Ambient window.
3. [x] Add native Ambient Home with folder selection, slideshow controls, weather freshness and shell exit; preserve gallery.
4. [x] Register in shell/default selection; test, build, back up, deploy and inspect actual device rendering.
5. [x] Record installed hashes, evidence and remaining limits.

- [x] Ambient full-interface orientation with shell/menu controls; native screenshot and shell touch verified.
- [x] JPEG frames and device timing measurements; exact readbacks and decoder guards tested.
- [x] Cinema black pixels plus verified BKL1 D4/off and D0 restoration; Bluetooth remained alive.

- [x] Native Backlight touch app with bounded OEM controls, portrait layout, shell entry, actual-C checks, native screen capture and byte-exact backed-up deployment.

## Firmware investigation — 2026-11-07

Scope: read-only probing, backup audit and offline reverse engineering. No firmware flashing, erase, boot edits or guessed hardware controls. Preserve current recovery and keys.

- [x] Audit existing local backups and classify file copies versus complete firmware/bootloader images.
- [x] Inventory available boot/startup/power and storage configuration using a bounded read-only helper.
- [x] Hash and archive collected evidence privately; record gaps and recovery prerequisites.
- [x] Inspect copied OEM startup/power components offline and identify evidence-backed next probes; full reverse engineering remains ongoing.
- [x] Write an initial firmware research report and next-session handoff with verified facts and unresolved hypotheses.

User requested Ambient Home opened in portrait first; leave it visible while research proceeds.

## Mero Terminal and two-way keyboard — 2026-11-07

User approved a persistent PC terminal, compact fastfetch, touch keyboard and PC keyboard forwarding into Foston apps. Firmware investigation waits.

- [x] Reuse signed live events/snapshots and inspect the local terminal startup.
- [x] Add a bounded persistent host terminal and explicit PC keyboard frontend.
- [x] Add native terminal app, touch input, orientation, shell/default entry and targeted keyboard reception.
- [x] Exercise actual host/native logic, build, back up, deploy and verify both input paths and background mode on hardware.
- [x] Document usage, installed evidence, permanent new-machine permissions and remaining limitations.

## Terminal latency and duplicate input regression — 2026-11-07

User reports very slow refresh and duplicated input with both physical and touch keys, including top. Inspect and reproduce actual delivery counts, remove avoidable polling/round-trip delays, test exact key delivery and interactive refresh, deploy backed up and record measured limits.

- [x] Reproduce mirrored input in actual reader tests and measure the current pipeline on hardware.
- [x] Fix input delivery and polling; add authenticated bounded compression with compatibility fallback.
- [ ] Test, deploy and measure on hardware; document actual behavior and limits.

User ended work for sleep. Preserve the open latency issue and resume from the top of docs/NEXT_SESSION.md. Current code is deployed and tests pass, but the latest isolated round trip remains6314ms; no reliable speed fix is claimed. Complete commits/push/handoff only tonight; no further experiments.

## Resume: portrait music and terminal control transport
- [x] Authenticate the current device and confirm fresh telemetry.
- [x] Implement music portrait layout, saved-orientation feed and rotated touch coordinates.
- [x] Build/check and inspect initial portrait device capture.
- [x] Deploy final tested music source and record backup/read-back.
- [x] Test real AgentSend packet bounds; install scoped 256-byte control PDU experiment with first-attempt timings through protected upgrade.
- [x] Measure three isolated terminal round trips and record first-attempt timings.
- [ ] Continue terminal responsiveness work: current median1330ms remains variable; physical typing confirmation pending.
- [x] Commit/push verified milestones and update handoff.

## Native-resolution music cover
- [x] Find upstream resolution limit: Firefox current export is60x60.
- [x] Prefer high-resolution song thumbnail; prepare bounded272x272 JPEG (current19,288B).
- [x] Add exact-dimension native decoding and versioned one-cover RGB565 mapping.
- [x] Verify native decoder, source selection, stale-track clearing and compression/feed regression checks.
- [x] Protected agent upgrade; stopped-app deployment/read-back; restart bridge.
- [x] Verify actual rendered cover through authenticated screen675/get676.
- [x] Record installed hashes/backups; commit and push.

## Further terminal latency work
- [x] Resume measured baseline:6092ms, one output. Prior sub-two-second results not stable.
- [x] Trace event arrival/ACK: first attempt fails locally before any packet, yet resets Bluetooth. Fix local retry without link reset and test actual worker.
- [x] Test and deploy negotiated compact foreground-terminal snapshots with generic-feed fallback.
- [x] RAM sender probes928/798ms versus fresh6092/6854ms baselines; first-send success/error diagnostics confirm staging stall removed.
- [x] Reject UI scheduling experiments: no proven benefit. Restore original terminal via signed rollback745/get746. Retained combination measured928/798/926ms with one output each.
- [x] Verify generic-feed return and Music capture, record evidence; transport commit2486955 pushed.
- [x] Save final handoff/verification and private evidence; optional human typing feedback requested.

## Keyboard-first terminal shortcut
- [x] Trace evdev grab, Hyprland bindings, keyboard ownership and native mode transitions.
- [x] Add persistent Super+Alt+Insert activation and grabbed-keyboard release; wait for shortcut release before capture.
- [x] Hide touch keyboard by default/on PC exit; explicit SHOW KB/HIDE KB control.
- [x] Poll physical input/ACK independently of display refresh while preserving lock/seat/error release.
- [x] Test/build/backed-up deployment; active Hyprland binding and device UI verified.
- [ ] Optional human test of the physical shortcut in both directions.
- [x] Update installation/handoff; Foston a1d0f35 and ArchMerOS a18817a pushed.
