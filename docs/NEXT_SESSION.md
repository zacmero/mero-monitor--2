# Foston handoff — 2026-11-07

## Latest installed state — 2026-11-07

Milestone source commits: `0f86777` (Ambient Home, native brightness and shell/file controls) and `d1080b5` (read-only firmware inventory). Full `rtk make`, `rtk make check`, optional probe builds and diff checks passed again before publication. Older entries below are chronological evidence; use this section for the latest installed state.

Ambient photos now center-crop to fill the whole screen in either orientation. Tap the weather widget to hide it; tap the same area to restore it. Taps elsewhere open controls. Backed-up deployment442 and exact readback444: 32,768 bytes, SHA256 `0cc787f516124cb723bd1e0738d79eded2f4ccbd33d2014f301e871ddf26d980`. Native tap446 hid weather; screenshot447/get448 verified an edge-to-edge portrait photo without controls or widget. Tap449 restored weather. Ambient remains foreground in portrait. Actual-C tap tests and full-frame crop checks pass. Status461 confirms the keyed agent remains connected, live age2 seconds, with weather/music feeds present. No reboot needed.

Read-only firmware research is recorded in [FIRMWARE_RESEARCH.md](FIRMWARE_RESEARCH.md). Existing backups are partial file collections, **not a complete firmware/bootloader backup or verified recovery image**. Preserved another private hash-verified archive, read selected live registry/storage/power configuration, and copied the factory `Windows/default.hv` through the supported file interface. No firmware, boot registry, navigation path or power policy was changed. Factory hive parsing and live system/user hive exports remain future work. No ScreenOff/suspend experiment has been performed. Minimum brightness works but remains physically bright, per user confirmation.


## User-confirmed brightness and power-return behavior — 2026-11-07

User confirmed the Backlight app works and that its minimum is physically still bright. This supersedes earlier uncertainty about whether the OEM brightness range has any visible effect; the remaining unsupported feature is full physical backlight-off while keeping Bluetooth awake.

User also explicitly confirmed: battery is dead; external power is required; reconnecting power enters charging mode only, and pressing the physical power button is required to start normally. Thus automatic Windows CE/shell/app startup on power return is NOT verified or working. Application autostart after CE boot is a separate, working feature. Charger-mode behavior points to the startup/power firmware path, but its exact implementation has not been established. No known safe software-only charging-mode-to-CE boot control was found. Do not change boot registry/bootloader or flash firmware under the app-work authorization. Any firmware setting or hardware power-button pulse solution requires separate evidence-backed investigation.



## Local Backlight control app — installed 2026-11-07

User requested a touch app to manually compare the native brightness range without vendor UI. Added wince/backlight/mero-backlight.c, `rtk make backlight`, and first-page Companion Labs entry Backlight (stable startup ID16; previous IDs unchanged). Buttons: DIMMER, BRIGHTER, MINIMUM, MAXIMUM, RESTORE (level at entry), SHELL (keeps selected level). Driver-reported level 0–10 and constant white/gray patches help visual comparison. Local controls work without Bluetooth once installed, follow portrait/landscape state and honor cinema blanking. No firmware, resident launcher, navigation path or startup preference changes.

Native app put/backup432, exact readback438: 14,848 bytes, SHA05d1c80ae2af0cef23360cc4a930d52f038e833a2a0d980bb539dfd9cf93563a. Shell0.3.6 put/backup435, exact readback439: 66,048 bytes, SHA17936f2198acb04a2f729fbb03c2880b0a40fb6016d2e77e154dd523388714f3. Actual app screen436/437 showed all six readable controls, patches and LEVEL0/10 in portrait. Left Backlight open for user testing. Physical minimum-vs-maximum difference remains user-observed; readback does not prove emitted brightness.

Root make, make check and diff-check passed. check_backlight_app.py compiles the actual C button actions and tests increments, both clamps, min/max, restore and unavailable/failed driver handling. Usage docs/BACKLIGHT_APP.md. Existing tuner and other apps preserved. Current work uncommitted; no pending job at completion.



## Direct manual minimum script — 2026-11-07

User requested the equivalent of lowering the native settings slider over Bluetooth, as a simple script. Added scripts/foston_backlight_min.py and native wince/probe/mero-backlight-min.c. Optional build target `make -C wince/probe mero-backlight-min.exe`. Installed helper put/backup427, 6144 bytes, SHA e1d1655f559c3198e25f43b86a2a3e2932a3dfebcb99ede9f43800261983b0c0. First actual script run launched a new helper and retrieved the process-bound report: pid2410375522, tick26750948, ok1, before19/request14/after14. Unlike earlier probes this action leaves brightness at minimum; it does not restore after eight seconds. Physical dimming still needs user confirmation; an async question is pending. Do not claim physical backlight off from this readback. Host stale/failed reply checks pass. Device remains at raw14 after this user-authorized manual action. No reboot or vendor UI was required.



## 2026-11-07 — physical backlight correction, shortcut and shell fixes

The user confirmed that BKL1 power D4 did NOT extinguish the physical LCD glow. Retract earlier physical-off claims: only returned power state and black pixels were verified. Reverse engineered the exact OEM Gui.dll (SHA dd68fde7d8ac585f96410706fad39709d014bd16498073c4a75629a1506bfccb), SetBackLight at VA 100145c8 calling 1001a440 and GetBackLight at 1000ac30 calling 1001a394. BKL1-only GET 0x22000c, DWORD output; SET 0x220008, DWORD input; vendor level mapping raw=14+5*level (0..10). Native driver reports zero bytes returned while writing a valid raw value; OEM checks BOOL, not byte count. Actual read/set/restore observed 19→14→19. User still reports unchanged physical glow. Physical dimming/off remains unresolved; do not claim it is fixed from readbacks. Driver registration is Drivers/BuiltIn/BKL, DLL buses.dll. Supported get of Windows/buses.dll failed (job 422). No guessed controls, raw GPIO, ROM dumping or CPU suspend were attempted.

Current agent 8934fcbdd1a1a208ac931b3c4b12510e81cb5acae86a492aa96c42ea457b7e2d, upgrade status 404; backup BT/AgentUpdate/old-1f46728bedebf6da.exe. It uses OEM minimum raw14 on cinema and restores the original raw value with a version-2 recovery journal. check_backlight.c tests actual code for zero-count reads, exact device/control/buffer bounds, save failure, invalid brightness and restoration after agent restart.

Root cause of shortcut mismatch: Super+Alt+S invokes archmeros-side-standby.sh→side-dpms.sh, NOT cinema-mode.sh. Bridge now follows DP-2 DPMS false as well as cinema-mode.json. Exact side script off/on integration verified black pixels and raw14 during standby, raw19 after restore. This is not proof of physical backlight change. Host monitor state was restored after testing.

Shell now rotates its entire layout and touch coordinates too (the previous toggle only controlled Ambient). Physical portrait shell screenshot and transformed touch were checked. Portrait explicitly saved as default at user request, with PC config backup preserved. Shell 0.3.5 colors: amber/coral/mint/violet/cyan/rose, tinted item panels, thicker highlighted default. Ambient controls also colored; selected folder amber. Shell put/backup407, exact readback408, SHA afc78e8902c1ba0428b997f937bacc3fe120878029bb1c5bb48b66f9d8151f71. Ambient put/backup410, exact readback411, SHA 74e4516b7809db7ba6160f3256024094205e60e143e7065c77e7c0eb1fa59380. Colors/portrait screenshot423/424. Left on portrait shell. Full builds/checks passed; work uncommitted.

Pending user clarification: whether the native vendor brightness slider ever physically changes this screen; do not require leaving our shell just to answer from memory. Docker is only the ARM compilation environment on the PC, not the device runtime; apps run natively on WinCE.



## Orientation, cinema backlight and compressed photos — verified

Installed shell 0.3.3: 4814b4cfbecc4ffbc7357a833d283c02933d7cec426166ace43e79266b6cbc9d, put/backup 365, exact readback 366. Ambient: 70b4e8bb6c1d2c83816ed05edb346bad885c36db53469d347221765cf14e08db, put/backup 368, exact readback 369, relaunched 370. Agent: be6a08442d84d28a402b876043beabdc25dbb6b3d2b5ed6eccccbfdc465e2648, verified upgrade 343, backup BT/AgentUpdate/old-49f9d693b0628339.exe.

Shell main 06 – ORIENTATION and Ambient menu ROTATE both toggle the saved PC portrait setting. The whole Ambient interface rotates, including touch mapping, weather, photo and controls; other legacy apps keep their layouts. Capture 350/351 verified rotated rendering; native main-shell tap 360 changed the saved choice back to landscape. Clean view contains no orientation button. Left running in landscape after tests.

JPEG transport is deployed: PC sends bounded compressed thumbnails using the existing content-addressed filenames; hash verified before JPEG decoding, strict 240x136 dimensions and existing RGB565 RAM mapping. Legacy raw frames remain accepted. Bundled decoder tests cover valid JPEG, truncated headers and invalid dimensions. Socket bounds remain 256-byte writes and 512-byte PUT PDUs. Status 326 measured raw 65,280 bytes / 7,806 ms GET / 11 ms decode; status 331 measured JPEG 10,046 bytes / 1,330 ms GET / 21 ms decode. These are different frames, not a controlled identical-image benchmark.

Backlight evidence: active Drivers/BuiltIn/BKL, name BKL1:, GetDevicePower result 0/state D0. Standard SetDevicePower D4 and restore D0 both returned success and reported matching states. The agent now follows cinema state with reversible BKL1 power-off, keeping CPU and Bluetooth awake. Original state is journaled in SDMMC/MERO/cinema-backlight.state before dimming and cleared by a zero record after restore; preserve this recovery journal. No guessed IOCTL, brightness registry keys, firmware or boot changes. Whole-device suspend/wake is unverified and not used.

Full cinema integration jobs 352–357 verified all-black captured pixels, BKL state D4 during cinema, D0 after marker removal, and uninterrupted authenticated Bluetooth operations. Temporary test cinema marker was removed. Evidence is private at ~/.local/state/mero/foston-evidence/20261106-orientation-cinema/. Full make/check and diff-check passed. Recovery-journal behavior after actual power loss has not been physically tested. Work remains uncommitted.


## Clean-view follow-up (latest)

Ambient app now defaults to full screen with weather only; tap anywhere for menu, FULL SCREEN to hide it. Loading captions and folder/filename strip removed; last photo retained during transfers. App hash e9fbf6536318c6ee0a84db76348905cac0effbd17c174d867416a27373ff7b3f, backup job 300. Clean startup capture jobs 302/303 verified. Agent upgraded and verified at status 309: 26ebc00bde41d8c80ed4efdcc55556d8d2ff5a34fbd21a602bde3a7e9f4ac200; backup BT/AgentUpdate/old-4e97579ac418acf9.exe. Native polling wait reduced to 0.5s and host publish interval to 1s, with transport safety bounds unchanged. Full checks passed. JPEG compression and throughput benchmarking remain pending; do not claim those are implemented.

## Latest installed milestone — Ambient Home

This section supersedes earlier installed/staged notes below. Ambient Home is deployed and running. See [AMBIENT_HOME.md](AMBIENT_HOME.md) for controls and PC-folder configuration.

- **Shell 0.3.2**: `a242e974d9034f4de3128a448d0dd9ec76e7f8ba77e1e4df1f438b201f82d160`, 63,488 bytes; put **273**, byte-exact read-back **294**. Ambient Home is first in Companion Labs and available as startup ID 15. Existing IDs and the saved YouTube Music default were preserved. The original gallery is named Local Gallery. Backup **258** retains the prior 0.3.1 shell; backup **273** retains the first 0.3.2 build.
- **Ambient Home**: `8256a604e767a54dc5c1e7dd5665e2c46a74b6c9ae90bf6b82644c2e3dc18560`, 32,256 bytes; put/backup **291**, byte-exact read-back **293**. Backup 291 retains the preceding acceptance build. No source photos were deployed to SD.
- **Keyed agent**: `edf15ccb370da4fcde25100b9a78786dc484285738e8b8e8c4448dac8b9b131d`; verified upgrade status **254** and final status **297**. Same navigation path; old agent retained at `\SDMMC\BT\AgentUpdate\old-5e5ab33eea18d983.exe`. Never commit the keyed binary/header.
- Host `mero-bridge.service` now supplies signed Ambient metadata/actions and bounded PC-resized frames. Configuration: `~/.config/mero/ambient.json`. Initial sources are Pictures and immediate subfolders (up to eight), with a 20-second slideshow interval after displayed-frame acknowledgement. The saved selection was restored to Pictures after testing.
- Native acceptance verified folder menu and actual selection of Screenshots, pause stable for 25 seconds, next while paused, resume, return to shell, and reopening Home (jobs 275–289). Actual captures show the selected photo, live weather and Pause/Resume; shell capture shows v0.3.2 and unchanged YouTube Music preference. A dropped tap during an outstanding heartbeat was found and fixed using one expiring local retry; extracted actual C tests cover contention, expiry and session binding.
- Final rendering capture **295/296** shows streamed Pictures, 26 C, animated sun and FETCHED 1m. Host logs independently showed another successful ten-minute fetch. Status **297**: live age 3 seconds, weather/music present, 38 MB free RAM, Launch50 unchanged. No unresolved job. Ambient Home is left open.

Gallery transfer is RAM-only: one 65,280-byte RGB565 frame mapping plus bounded decoder/display buffers, exact hash/length validation, foreground demand and clearing on app exit. Existing Local Gallery remains local; do not confuse it with the new streaming Home. Frequency Matrix is still separate. Additional PC controllers and agent dashboards are deprioritized by the user.

`rtk make`, `rtk make check`, and `rtk git diff --check` are the required final checks. Native Ambient cold-power/default startup and cinema standby have not been physically retested in this milestone; shared startup/standby code is reused. No boot registry, firmware, resident recovery or pairing changes were made. Current work is uncommitted, including the earlier file-controls fix; preserve it.


## Earlier resume update — before Ambient Home

Latest agent: `68bb414de26889d65b26e179b9ab601c602eeb8d9a72b7e1bf129c0224b06c85`.
Agent mode now queues manual list/download/upload on its existing worker rather
than disabling file controls as permanently busy. Connect automatically lists;
the UI shows a fresh heartbeat and uses buffered painting. Actual file-list
rendering and exact 42-byte test upload were verified with agent ON. Download
dispatch has an offline test; this latest UI acceptance did not download a file.

The user reported startup directly into Music with Bluetooth and correct data.
Status 195 independently confirmed fresh telemetry. The prepared update was
then deployed: shell **0.3.1**, hash
`df7c8248619a222c94ded2e8f1c77b769a2ed6f556569d748136099149d4c93a`,
backup job **203**, retrieved byte for byte; agent hash
`22c494eba344ecef15939e2af2a1cdff2bccd6c3f2aa4da5c905afec41100422`,
old agent retained at `BT/AgentUpdate/old-5a9c8be03aff0293.exe`.
Status 206 still showed live data age 2s after shell startup. A native capture
confirmed 0.3.1 and the saved YouTube Music default. The shell connect-button
acceptance and another physical power-cycle test are pending. The older
installed/staged descriptions below are historical context for this update.

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
