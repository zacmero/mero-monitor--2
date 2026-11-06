# Foston handoff — 2026-11-06

The user stopped for the night and authorized committing/pushing this state.
Device operations were finished with **no unresolved agent job**. Do not treat
an offline device tomorrow as a new fault before confirming it has power.

Code milestone: `cd73d96` (`feat: add live Bluetooth companions`). Read
[AGENTS.md](../AGENTS.md) for authorization, secret handling and recovery rules.
Prefix shell commands with `rtk`. Work on mero-machine in this repository.

## Installed and verified

| Component | Installed state |
|---|---|
| Shell | **0.3.0**, SHA256 `d07e01cf6e9acc9ccf0353bb07bf50f8971e7469e0f824b6d2b270d08921dfc7` |
| Keyed agent | SHA256 `888efc5f3b6c7229cfaf0a1cb750efb37f646d56317a32891e1ce418481762c9` |
| Agent/navigation path | `\SDMMC\BT\Downloads\101907-mero-agent.exe` |
| Music deck | SHA256 `34ec83522803fd1ee4ec4c143bee335de0ce1319449d71a2ea1eaad245e7cbec` |
| Boot registry | Read as `Launch50=\ResidentFlash\MERO\mero-shell.exe`; unchanged |
| Last USB inspection | Default `Serial_Class`; active `COM6:` from the Serial_Class function driver |
| PC services | `mero-bt-ftp.service` and `mero-bridge.service`, user services |

- Authenticated status/list/get/put/rollback and app launch/stop work over
  Bluetooth, without USB mounting. Payloads and retrieved files are hash checked.
- Weather shows fresh Farroupilha conditions, three forecast days, animation,
  fetch age and an offline/cache state.
- YouTube Music shows actual track timing and matching thumbnail. Native
  Play/Pause -> Bluetooth -> PC MPRIS worked. A bridge restart did not replay
  an acknowledged toggle. The user also confirmed Play/Pause works.
- Artwork occupies one RAM slot, with a reused display buffer. Old pixels are
  cleared when new track metadata arrives and on close. No new cover files are
  written on the device SD. PC thumbnail caches are limited to four files.
- Companion Labs contains the app catalogue. Default-app selection persists.
  A temporary Weather default auto-started after a shell restart; closing it
  returned home without a relaunch loop. Its previous preference was restored.
- Standby now uses an explicitly painted black window: an actual capture was
  all black. With updates stopped, the screen recovered and showed the cached
  deck offline. This is software blanking, not hardware backlight/suspend.
- Gallery, tuner, camera, CMD, probe and the ordinary BT client were updated
  and read back. Exits return to the shell. CMD's automatic recovery mirroring
  and destructive fullscreen/PhoneLink sweep were removed.

Hardware captures and manifest: `~/.local/state/mero/foston-evidence/20261106/`.
Agent receipts/backups: `~/.config/mero/foston-agent/jobs/` and device journals.
Do not reset or delete the key, sequence, pending job, receipts or journals.

## The startup regression and recovery

The first user power-off/on test kept shell 0.3.0 and the saved YouTube Music
default, but did not launch music. Bluetooth connected briefly then dropped.
The user reported USB had returned to Mass Storage. The shell's BT action gave
the misleading instruction to download the already installed transfer app.

After the user changed USB settings in the vendor UI and reset, they reported
the default app working. Bluetooth was then independently confirmed: pending
read **177** was resolved with `wait`, and fresh status **178/191** showed live
weather/music at age 2s. Native inspection showed **Serial_Class / COM6**.

Do not claim the exact cause is conclusively isolated. Mass Storage previously
blocked device SD writes, and startup ordering/manual-client confusion were
also found. A true USB-power removal/reapply test is still needed; a power
button may suspend/resume instead of cold booting.

## Prepared source — NOT installed yet

The user needed to turn the device off, so no further deployment was started.

**Shell 0.3.1** is built, SHA256
`df7c8248619a222c94ded2e8f1c77b769a2ed6f556569d748136099149d4c93a`.
It includes:

- **Connect / Agent** under Bluetooth Link, launching the saved keyed path;
  no fallback to the plain transfer executable or false download-missing message.
- Separate keyed-agent window `MERO_FOSTON_AGENT` and ordinary-client window
  `MERO_BT_FILES`. The installed agent already uses the new keyed class.
- Startup agent retries while storage/WinCE networking becomes ready; default
  app launch retries rather than failing after one early attempt.
- Deferred SD preference loading without overriding an explicit user cancel.
- OEM-evidenced ActiveSync setup before agent/app startup and on manual Connect.
  It preserves an already active Serial_Class link instead of resetting it.
- A heartbeat-based link status rather than merely checking for an OEM window.

The last agent source change clears a queued reconnect when the user explicitly
pauses the agent. That small cancellation fix is also **not deployed**. A built
candidate is private at
`~/.config/mero/foston-agent/candidates/20261106-startup.exe`, SHA256
`d100ffe6b56ad20e4c3676e75935101094937d922542b01570f9a39071842f19`.
Use the supported `upgrade` flow, not a normal put over the running executable.

## USB control evidence and limits

The old shell used guessed `0x220008` and did not verify a runtime switch.
The new [mero-usb.h](../wince/mero-usb.h) follows the device's actual Setting.exe:

- Private OEM archive:
  `~/.local/state/mero/foston-oem/20261004/ResidentFlash2/APP/Setting.exe`.
- SHA256 `0944abf83e10e7ee00066a18d1f2a11eae6b65eb4a19a8baf0264378cf6a06a4`.
- ARM routines `0x15a50` (controller lookup), `0x15bd4` (default query),
  `0x15c9c` (default/runtime change), `0x15eac` (mode 1=Serial, 2=Storage).
- Interface GUID `E2BDC372-598F-4619-BC50-54B3F7848D35`; FindFirstDevice
  by GUID, 1584-byte device info, open the returned **bus name**.
- Default query `0x220014`, 756-byte output; default set `0x220018` and
  runtime change `0x220010`, each with a 256-byte client-name input.
- The OEM restores the previous default when runtime change fails; our code
  does likewise. Only Serial_Class and Mass_Storage_Class are accepted targets.

The read-only query was actually run and returned Serial_Class. Mode changes
and rollback have **offline tests only**. Do not report hardware switching or
cold-power reliability as verified yet. No new USB mode writes were made by
this implementation before shutdown. Never substitute guessed HAL/driver calls.

The read-only `mero-boot-probe.exe` writes `/SDMMC/MERO/boot-state.txt` with USB
defaults, active drivers and process names. Its latest source can be rebuilt
under `wince/probe`; it does not change USB or boot settings.

## Resume sequence

1. Confirm the device has power. Keep pairing unchanged.
2. Run `rtk proxy python3 scripts/foston_agent.py --timeout 30 status`.
   On timeout, preserve the pending read and use `wait` after reconnecting.
3. If the old shell cannot reach the agent, use the vendor USB selector to put
   it in ActiveSync, then launch the exact saved keyed navigation path above.
   This is recovery for the old installed shell, not the intended final flow.
4. Deploy the staged agent with `rtk proxy python3 scripts/foston_agent.py upgrade`.
5. Back up/deploy/read-back shell 0.3.1 through the agent and launch with
   `launch mero-shell.exe --home`. Do not alter Launch50 or resident recovery.
6. Verify Connect / Agent from the shell, including an explicitly paused agent.
   Confirm an already active Serial_Class link is not reset by startup.
7. With the user present, test actual power removal/reapply: saved Music default,
   automatic Bluetooth, and ActiveSync without vendor UI. Record results.

`rtk make`, `rtk make check`, keyed-agent compilation and whitespace checks all
passed for the saved source. Checks cover actual C crypto/protocol, partial
writes, rollback boundaries, live receiver/cinema recovery, music timing/cache
invalidation, startup retry/mutex cleanup, and OEM USB sizes/codes/rollback.

Future ideas remain in [COMPANION_ROADMAP.md](COMPANION_ROADMAP.md): six-button
PC control, VM activity relay, adaptive gallery/ambient rotation, focus/inbox,
real audio spectrum and outdoor GPS logging. Prioritize finishing startup first.
