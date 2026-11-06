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
- [ ] Deploy shell 0.3.1 and the final agent pause-cancellation fix, then validate hardware mode switching and cold-power startup.
- Paused overnight at the user's request. See docs/NEXT_SESSION.md for exact installed and prepared builds.
