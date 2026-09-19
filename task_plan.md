# ActiveSync media synchronization

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
