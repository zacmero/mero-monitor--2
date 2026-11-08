# Latest checkpoint: Ctrl+Space forwarding repaired (2026-11-07)

User could not detach an accidentally opened Herdr client. Static Herdr config uses Ctrl+Space then lowercase d. Host Layout generated00 correctly, but native keyboard validation rejected NUL before acknowledgement; touch CTRL+Space also emitted ordinary space. This was a forwarding bug, not a reason to stop Herdr sessions.

- Agent now accepts NUL only for the exact foreground Mero Terminal, including resolved MERO_FOREGROUND. Nonterminal packets containing NUL reject before any character is posted. Signed durable intent/dedup, malformed UTF-8 checks and foreground checks remain intact.
- Touch CTRL+Space/@/2 emits00 through the existing signed terminal bus. Host tests exercise actual native NUL delivery/dedup, other-target whole-packet rejection and touch hex framing.
- Protected agent upgrade through status771: SHA256 `1b857bc49f90f037b35976ba0a8b09a55636d2885abf3d69a58e55a578727010`; prior095a52e7… agent backup `\SDMMC\BT\AgentUpdate\old-c105d08e76025a12.exe`.
- Terminal stopped764, put772/get773:31744B SHA256 `b3e2ad0879f44d992b0ec080aab5dbacdf8e0501cc9c0e8f1ce58abda60198e0`. Backup772 preserves00da812c… shortcut/hidden-board app. Launch774 returns to the persistent PC session. Verify the final job numbers in receipts if interrupted.
- Full make/check and whitespace checks pass. Fix milestone44bf43d pushed. No Herdr CLI session control or process termination was issued. User confirmed the updated Ctrl+Space → lowercase d sequence returned to Bash. Super+Alt+Insert activates forwarding if off; release Ctrl+Space before lowercase d, then use Super+Alt+Insert to return keyboard control to the PC.

---

# Current checkpoint: keyboard-first terminal (2026-11-07)

**Start here.** Super+Alt+Insert is configured to open the Foston terminal and toggle PC keyboard forwarding. The same chord releases forwarding inside evdev capture, since Hyprland cannot see grabbed keys. Touch keys start hidden and remain hidden on PC-mode exit/error; SHOW KB / HIDE KB is explicit. No host window or extra daemon was added.

- Source milestones pushed: Foston `a1d0f35`; ArchMerOS `a18817a` (Lua + legacy binding, executable wrapper, keybinding docs).
- Terminal stop755 / put756 / get757 / launch758: 31744 bytes, SHA256 `00da812c39768bf87468d97b4eeaf661daf146ab6b4c6d592a896a08f7c1691d`. **Backup756 restores the prior verified18d00491… terminal.** Agent remains095a52e7…; no agent/firmware/boot changes in this milestone.
- Screenshot759/get760 visually confirms SHOW KB, PC KEYS and full terminal view with touch keys hidden after PC mode exit. CaptureSHA `616ce0a4555004b1dd8ab6b84cc94cf68617043e6540ae3cd8563b42756e4a54`.
- Host shortcut client uses the existing mode0600 socket, launches only `mero-pc-terminal.exe` through the signed agent, then arms capture. It waits for the shortcut keys to be released and fresh signed foreground-terminal presence. Offline/failed/unknown launch never grabs or resets pending jobs. A nonblocking local lock suppresses overlapping launcher processes. Arming can be cancelled or times out after10s.
- Shortcut mode targets `MERO_TERMINAL` explicitly, preserving per-packet device foreground checks. Capture releases on lost fresh foreground confirmation, seat/lock/input errors, timeout, PC OFF, Super+Alt+Insert or Ctrl+Alt+Escape. Existing manual PC KEYS mode retains supported-app forwarding. Durable keyboard journals/deduplication remain unchanged.
- Bridge polls input/ACKs on its existing25ms loop and requests publication immediately when pending input/mode changes. Seat/lock verification targets100ms while capturing. Foreground/device presence uses the existing8s authenticated capability lease; no tighter3–4s cutoff that could fight the3s refresh cadence.
- Diagnostic PC-input→PC-published-output samples, **not final device paint**: baseline959/1057/889ms (median959), new834/944/932ms (median932). No major total-output improvement established. Device deliveryACK median572→451ms (baseline486/575/572; new351/470/451). Small samples and different endpoint from the earlier CE input→received-frame probe.
- Initial benchmark sample appended to pending 'ok' prompt text and failed as a shell command; discarded. The harness now checks for an empty prompt before every harmless marker command. Do not inject commands blindly into a user's unfinished command or running program.
- Current Hyprland runs `-c ~/.config/hypr/hyprland.lua`, symlinked to ArchMerOS/config/hypr/hyprland.lua. Binding exists in Lua **and** legacy.conf. Active reload/configerrors passed; exactly one Insert/modmask72 binding present. Super+Insert/modmask64 remains the PC on-screen keyboard shortcut.
- Wrapper: ArchMerOS/config/archmeros/scripts/archmeros-foston-terminal.sh, executable through the active config symlink. Defaults to `$HOME/projects/mero-monitor-#2`, overridable via `MERO_FOSTON_REPO`; invokes `scripts/foston_keyboard.py toggle-terminal`. Existing permanent keyboard udev setup is sufficient, no new sudo step.
- Private benchmark/UI evidence: `/home/zacmero/.local/state/mero/foston-evidence/keyboard-shortcut-20261107-221441` (JSON timing samples, screenshot and SHA256 manifest).
- Full make/check, native hidden-board parsing/control tests, held-key arming/timeout/foreground tests, actual Layout release-chord test, local client no-window/launch-failure tests and Bash/config validation passed. Human physical shortcut test requested; **do not claim a real two-way keypress test until user confirms**. Terminal left open, forwarding off at last observation.

---

# Latest checkpoint: terminal transport latency repaired (2026-11-07)

**Start here.** The final retained change is commit `2486955` (pushed): transient signed input/codec messages send directly from RAM, and the foreground terminal uses a smaller signed snapshot. UI timer/posted-flush experiments were rejected and the original terminal was restored.

## Evidence and outcome

- Fresh pre-fix isolated tests: 6092 ms (launch679/get680) and 6854 ms (launch682/get683), one output each. Compact-only test remained6591 ms (693/694).
- Status695 proved the first attempt failed before any OBEX packet. The intermediate local-retry build measured1713 ms (704/705); status706 identified `ERROR_DISK_FULL` (112) on the CE Temp staging write, despite39 MB freeRAM/6807 MB freeSD. The previous error branch also disconnected the healthy link.
- RAM-only sender with the original terminal: **928,798,926 ms; median926 ms**, each with one output. These are small-sample input-to-authenticated-RAM-frame measurements, not final-paint timing or a frame-rate guarantee.
  - launch715/get716: reportSHA `164db500d13b3c0e221f1f6683ec475912a18945c128acbeca2f83806f3ffaf9`.
  - launch718/get719: reportSHA `cbcfec4fbaf49421204cb1eb7b921ec64f1b7ac74b8ef30878d6a4df1e7f00b3`.
  - final restored-app launch748/get749: reportSHA `fb1df826248f1dcb06608c95fd2c5996f3957f1dbc9bbf513d10ceabd2ed0e2b`.
- Status717 confirmed first-send success,2 bounded packets, OBEX success160, staging0/error0. Status751 after returning to Music confirmed the same agent, successful first upload, generic weather/music flags1, age1.
- Host observer measured154–207 ms event arrival→publishedACK on the slow runs. The large stall was before PC event arrival. Underlying CE storage capacity was not repaired/cleared; transient events no longer depend on staging files. Durable keyboard/agent/deployment journals remain unchanged.

Private durable evidence archive: `/home/zacmero/.local/state/mero/foston-evidence/terminal-latency-20261107-214834` (timing reports, Music capture and SHA256 manifest; no keys or keyed executables).

## Exact installed state and rollback

- **Agent** protected upgrade through status713: SHA256 `095a52e746a8f0b21214bda20297bacacfd76121d09c0131c920bdf40fc9ccb0`, same key/navigation path. Prior intermediate backup `\SDMMC\BT\AgentUpdate\old-cf5f69f1bfe802ab.exe`. Pre-session high-resolution-art agent5d1997b7… is preserved at `BT/AgentUpdate/old-c97b39c17c98f9cc.exe`. Use the protected upgrader for agent rollback; do not overwrite the running agent or reset state.
- **Terminal** original source restored. Stop744 / signed rollback745 (reference721) / get746 verified31744B SHA256 `18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60`. Experimental versions remain in deployment backups; they are not the final source/device state. A fresh local build may have a different PE timestamp/hash; do not confuse it with the installed verified binary.
- **Music** unchanged from the verified native272-square artwork milestone: SHA256f3b83d84…, one bounded RGB565 RAM cover, JPEG source preferred over the60px Firefox export. Default orientation and saved startup app unchanged.
- **Bridge** restarted with compact publisher; normal app feed remains `live-state.txt`. Terminal foreground uses `live-terminal.txt` only after signed advertisement, retaining ACK/keyboard/job/codec fields. Missing files or no advancing frame for2s fall back to generic, including older-host rollback. Compact mode normally has music/weather flags0; that is expected. Generic Music status751 restored both to1.
- Socket writes/control PUT PDUs remain256B; bulk PUT PDUs512B. Host execution receipts and device durable keyboard journals preserve original safety/flush requirements. No firmware/boot/USB/pairing changes.

## Checks and limits

Full make/check and whitespace checks pass. New checks compile the actual file/RAM sender, verify exact bytes and both packet limits, exercise actual event-worker retry/ACK behavior, and test signed compact-frame parity plus native stale/absent/generic fallback. The terminal UI source/tests are restored to their original implementation.

Timer-only samples1071/1043 ms still queued input~123 ms. Eager posted flush split bursts (1723/1586 ms);10ms batching measured1067/1072 ms. No measured benefit, so none of these UI changes were retained. Do not repeat them based only on requested timer intervals. Physical typing experience still needs user feedback; no new keyboard window or permission step was introduced. If the durable keyboard path reports a write failure, diagnose it without deleting its journal or weakening delivery intent.

---

## Current artwork checkpoint: native272x272 (2026-11-07)

The prior112-square notes below are historical. Firefox's current MPRIS export was only60x60. The host now prefers matching high-resolution song artwork, prepares272x272 JPEG (quality92, lower only to enforce60000B cap), and retains the restricted Firefox file as offline fallback. Current cover is19288B, smaller than the old25088B raw transfer. Host owned caches remain bounded; no cover files are written to the Foston.

- Agent authenticates encoded hash then validates exact272x272 JPEG dimensions before allocation/decoding. One147968B RGB565 cover in versioned `MERO_ART565_V2`; music drawing cache is bounded to the same image. Old artwork clears on track changes/app close. Ambient still requires240x136. Socket/PDU limits unchanged.
- Protected agent upgrade through signed status671: SHA256 `5d1997b73b34fdde90ff4048755479d2f894129e3606fad8ae6f29299a454744`; previous agent backup `\SDMMC\BT\AgentUpdate\old-52462ed6200a1cda.exe` (old agent uses112-square V1). Navigation path/key unchanged.
- Music stop664/put672/get673/launch674:180638B SHA256 `f3b83d8469aaf1cea8c1c2bd253def9083f34b314302c0da780b6968435c173d`, previous binary backup672 (112-square ABI). Rollback must restore matching agent/music/host formats together; a code-only rollback is insufficient.
- Bridge restarted with new JPEG publisher, both services active. Actual screenshot675/get676 visually confirms sharp full-width album cover and working track/timing feed (Korean Bodega / Fun Lovin’ Criminals). Capture SHA256 `9a7e142ec015b48854ae513f479f3c1f709747cff6a66dba901db8b8235d3111`.
- Full make/check and whitespace checks pass. Tests cover high-resolution preference, undersized network rejection, source preservation, offline file fallback, reused export path, stale track handling and actual native decoder dimensions/truncation/encoded-size bounds. AGENTS artwork contract now reflects272-square single-cache limit authorized by the user's resolution request.

---


## Latest music update: edge-to-edge cover (2026-11-07)
Portrait artwork now fills a272x272 square across both screen edges. Track text, progress and controls sit below it; landscape and the single112x112 RGB565 RAM source are preserved. Stop658/put659/get660 verified180638B SHA256 `93f73cf9da319731d1203485905f4112ce31d569a4818cfc5106bee3f3f1f973`; backup659 restores the prior portrait deck. Launch661 and screen662/get663 verified actual playing artwork (Up on the Hill, Fun Lovin’ Criminals), full-width cover and unobstructed controls. ScreenSHA256 `8e0384b11e19d49a97cdc70598d22222bdd8b07af153347e22e5cd24ebc6c35b`. Full make/check and whitespace checks passed.

# Current checkpoint: portrait Music and measured terminal transport (2026-11-07)

Resume here; the overnight STOP marker below is historical. User resumed terminal work and requested Music follow the device orientation.

- Music milestone `0e542bc`: follows authenticated AMB portrait setting, full 272x480 logical layout and inverse touch mapping; no extra app orientation toggle. Artwork remains one bounded 112x112 RGB565 RAM cover. Default app/config/boot settings unchanged.
- Final music stop641/put642/get643:179936B SHA256 `8fe7fdca283886e6821bb62a71c097fba6e598450c4552441c05f99f28f791d8`. Backup642 retains the intermediate portrait build; **backup626 retains the original landscape build**. Initial screen629/get630 confirmed portrait layout. Launch655 opens final source build; screen656/get657 exactly matches the inspected portrait capture (SHA2561677f9059d95636146c45a6b080cf10e960a2d3efc461a31e6073c9cc7586c7f). No active PC music session during capture; playback/artwork were not reverified today.
- Transport milestone `454cfe4`: small control PDUs256, bulk512, socket writes256. Protected upgrade through status638: agent SHA256 `00580b7a3e7ae1c935da9081330b93dfd9f9c3dca0c96342bea3f144e50bd1a0`, prior agent backup `\SDMMC\BT\AgentUpdate\old-b2bf6449c1d858a2.exe`. Same navigation path/key; no firmware/boot changes.
- Agent status now reports `event_id`, `event_queue_ms`, `event_write_ms`, `event_first_send_ms`. These retain the first attempt rather than masking it with later retries. Actual-AgentSend host harness covers both caps, byte-exact bodies, empty files, small-peer and exchange errors.
- Updated latency probe put644:19456B SHA256 `11ec0f2ec21302e8b823f7aed04d2e7a3801474f6d3981ce1e51675a0868c735`. It now writes `ok=0` if terminal is not foreground, preventing stale success reports. **After agent upgrade, explicitly launch terminal before measuring**: agent startup may take foreground. Launch639 was invalid and GET640 matched yesterday; excluded.
- Three isolated fresh probes (10s deferral, no RPC during measurement):
  - launch646/get647:2103ms, queued125, ACK2103; reportSHA `780ace857ddbc3fa1e2654d2f1d9d61ee00fe97574fba2f120d5299c03cf63f8`.
  - launch649/get650:941ms, queued121, ACK941; reportSHA `870517ef8861ae848cefd495b683ef6f442b2fbd921a251afe25e6c3fb64a264`.
  - launch652/get653:1330ms, queued119, ACK914; reportSHA `50bf11f1e8f3ce7a659c5301441c7013d7c4d745951a34a8aa028da760d978b3`.
  - All output occurrence counts1. Median1330ms; this measures authenticated RAM frame receipt, not final paint. Variable, still not an instant terminal; no controlled before/after speedup claim. Yesterday's6314ms remains a historical observation.
- Status654 matched last event ID: queue109ms, Temp write1ms, first OBEXsend263ms, liveGET359ms, frame1399B, period503ms, live age0. A host file observer during third run saw154ms event arrival→publishedACK (diagnostic only, no commands/keys logged). Remaining latency comes from transport/publication/frame scheduling and should be investigated before further architecture changes. No SPP connection has been tried.
- Terminal binary remains prior verified18d00491…; background Bash retained. Physical capture off. Human confirmation of physical/touch key duplication remains needed; native duplicate/touch-guard tests and all three injected round trips pass.
- Full `make`, `make check`, new sender test, music orientation tests and whitespace check passed. Generated/keyed binaries excluded from git. User services remain active. Device stays in ActiveSync/PC Link.

Next terminal work: collect multiple actual typing/paint timings; preserve deduplication, active-seat/lock release and no-window keyboard operation. Consider a controlled old/new packet-cap comparison if needed; do not infer causality from the small variable sample. Keep the Music portrait milestone available independently.

---

# Foston handoff — 2026-11-07

## STOP HERE: overnight checkpoint — terminal latency still unresolved

User explicitly ended work for sleep and requested commits/push plus tomorrow's handoff. Resume **perfecting Mero Terminal**, ahead of firmware research. Do not describe the speed problem as fixed. The user reported very slow refresh and duplicated top/keys with both physical and touch input.

Source checkpoints: `66a5290` (terminal, seamless keyboard and transport work) and `0d694fb` (isolated latency probe). The following handoff commit preserves the exact installed state and open investigation. Full root make/make check and whitespace checks passed before publication. User requests continuing to commit/push meaningful future milestones, with device backups as well as Git checkpoints.

Latest installed/verified state:

- Agent upgrade595/status623: `9b3488c3aa3cf4a0ca852eb750567f8b390774b823a5ab366514cd07e295a0df`; agent backup `BT/AgentUpdate/old-7172786efe30cb64.exe`. Navigation path stays `SDMMC/BT/Downloads/101907-mero-agent.exe`. The CLI intentionally archives/removes the local keyed build after upgrade; absence of wince/bluetooth/mero-agent.exe does not mean the device lost it.
- Terminal put/backup597, exact readback598, launch599:31,744 bytes, SHA256 `18d00491984e3b6bd86fdfe07fde704691b65969c6def4adccd78dc910d77e60`. Source and device match. Shared layout is unchanged, portrait currently32×26 text cells. Physical capture is OFF; no desktop keyboard window. Bash session is retained.
- Shell0.3.7 and other keyboard-enabled apps remain the versions listed below. Default startup preference, Launch50, recovery copies and pairing were preserved. Status623: agent_ready, live age0s,38MB free; live GET393ms, accepted-frame period528ms, wire frame1401 bytes. Those are reception metrics, not a promised UI frame rate.
- The last signed status read623 completed and retired normally. Verify no pending.json before starting tomorrow; do not reset its sequence. The temporary pending file observed during that status call was the in-flight read itself.

Implemented and tested, with current limitations:

- The real PC Bash/tmux terminal, compact **ArchMerOS** fastfetch, touch keyboard, portrait/landscape, shell/default entry and persistent PC session work. A PC→Foston→PC command round trip and portrait touch pwd were previously verified.
- Seamless physical keyboard capture stays inside mero-bridge. No extra Hyprland/WezTerm window is wanted. User completed permanent scoped udev setup; uaccess tags and active-user ACLs were verified. See INSTALLATION.md for new machines. PC KEYS hides touch keys; TOUCH releases grabs. Do not revert to temporary per-session sudo/setfacl as the normal solution.
- Both CASUE interfaces advertise every typing key. Capture grabs both but selects one actual typing source, filters repeated DOWN/orphan repeat, and keeps genuine held-key repeats. Native touch uses one DOWN per UP/focus-loss, plus pressed-key highlighting and a SENDING footer. Actual reader/native C regression tests pass. Human confirmation after these fixes remains useful; do not claim all duplicated-key reports are proven resolved on hardware.
- Input is processed before capturing the outgoing frame. Music polling runs separately. Foreground presence is carried outside the one-command input mailbox. Pending device input uploads before screen GET; duplicate uploads are cached. Signed job-sequence hints avoid idle job GETs, with timed fallback if hints are missing/stale. Original command authentication, durable sequence/replies and unknown-outcome rules are unchanged.
- Zlib snapshots negotiate via signed per-bridge nonce/epoch leases. Inflation uses the existing stb decoder, checks checksum, rejects NULs and stays within4096-byte output. Old/rolled-back agents fall back to plain state after lease expiry. Socket writes remain≤256 bytes; PUT PDUs remain≤512 bytes. Tests cover actual native decode, malformed data/output bounds, lease fallback and scheduler/tick wrap. ASCII output/no ANSI colors and incomplete dead-key composition remain initial-build limits.

### Findings that matter tomorrow

**The latest isolated test still took6314ms from application input to received output frame**, with queued137ms and ack5786ms, exactly one output marker. Private report: `/tmp/foston-terminal-deploy/latency-isolated.txt`; get619, SHA256 `2b1310fdd4469ffd061479652a38afa30478b25b621c35b1c761fb47d3934e8c`. The helper deferred injection10s; host launch RPC finished at1.511s, and no other agent RPC ran until36s. This supersedes the earlier hypothesis that launch journaling alone explained the delay.

Host-side observation for that same run: event arrived at16.996s after launch, with created-age6.516s; PC output was published at17.158s, only162ms later. The long delay is principally **between the native queued event and its arrival on the PC**, not ordinary Bash execution or PC frame generation. Received frames otherwise progressed around0.4–0.5s. No stable input-speed gain has been demonstrated; do not quote provisional earlier runs as a successful before/after benchmark.

The native filesystem timing helper also ruled out the simple Temp-flush theory: Temp create0/write3/flush0/close0ms; SD create4/write5/flush0/close118ms. Private report `/tmp/foston-terminal-deploy/file-timing.txt`, job622. That temporary helper is not a firmware probe and only wrote its owned scratch/report files.

Next focused investigation:

1. Instrument first **AgentLiveEvent** send start/end and queue-to-send timing, rather than repeatedly reducing scheduling intervals. Existing live_send_ms can be overwritten by a later retry and does not show the first attempt's full history.
2. A promising **untested** packet hypothesis: AgentSend permits512-byte PUT PDUs while TransferBytes splits writes at256 bytes. Short control events can therefore span two native writes. Test a256-byte PUT-PDU cap **only for control files** (live-event, keyboard-result, live-codec), retaining512 for bulk transfers. This would stay inside existing bounds; it was NOT applied before sleep. Check transport behavior and signed replies, not just compilation.
3. The diagnostic probe polls the shared mutex every10ms; an observer effect from zero-timeout lock contention is another unproven hypothesis. Compare a lower polling rate and add per-event timing if needed.
4. Re-test one actual touch press (including release) and physical typing, then top. Rebuilt `mero-terminal-touch.exe` (put571) sends three DOWN messages followed by one UP per key to test suppression; it has not yet been run against the final build. Old test helpers that emit only DOWN are invalid after the new press guard. Local corrected mode/PC-key helpers are in `/tmp/mero-terminal-touch/`; they were compiled but their updated versions were not deployed.
5. Only consider a dedicated RFCOMM stream after locating the delay. Existing OpenPeer documents verified AF_BT32/RFCOMM3,40-byte address and UUID layout. dbus/gi exist on the host. No SPP profile registration/connection experiment was performed. Never guess hardware IOCTLs or flash firmware to solve this app issue.

`mero-terminal-latency.exe` is installed (put614),19,456 bytes, SHA256 `b68fb565683ecaa3de8ff9dd7bfe3ea1645a83ced3254f8f193863376e60c6e8`. It waits10s, injects a unique harmless printf and reports input-to-RAM-frame reception. Let it finish (up to30s total) without agent jobs, then retrieve terminal-latency.txt and verify a fresh ID. The first deferred driver read too early and received an old report; freshness checks correctly rejected it. Do not re-launch merely because a read was stale.

### Checkpoint and rollback

Build/tests, native helper build, rule validation and whitespace checks are recorded in the checkpoint commits. Generated/keyed executables, OEM files, device journals and private evidence remain outside Git. **A Git checkout/revert does not roll back the installed device.** For public apps, stop the named app, use the signed PUT backup/rollback reference or rebuild and deploy with backup/readback. For the keyed agent, use the protected upgrade workflow with the existing key; never copy over/kill the live agent, reset counters or discard pending jobs. Preserve all current recovery and configuration files.

## Mero Terminal and seamless keyboard — latest 2026-11-07

User prioritized this app before further firmware research. **01 – COMPANION LABS → Mero Terminal**, new startup ID17; old optional terminal and prior startup IDs/default preference preserved. Shell0.3.7 deployed474/readback475: b6e78849630d19dd6b2cfaddb59c246c0dec162462adf0efc870abfc0840e3cd,66,560 bytes. Terminal final put/backup529/readback530: 8ec22febaee2888c31f1c913d99d85ff4834195a015ad68b151ec94f5cd4c651,30,720 bytes; launched531. Firmware work remains read-only and deferred.

Agent upgrade status521 verified d94c8f18fbc851b5f22d236b0553e4bd53d8d0f77432bfb2fa81986838374866; backup BT/AgentUpdate/old-0996c52f2a19c7b2.exe. Native keyboard delivery is limited to supported custom foreground classes, journals intent before delivery and signs acknowledgements. Generic foreground selection still rejects OEM/system/agent windows. No boot/nav configuration changed.

PC KEYS engages physical keyboard forwarding inside the existing bridge; TOUCH releases it and restores the native touch keys. **Do not open an extra Hyprland/WezTerm forwarding window**: the user explicitly rejected that earlier approach. Keyboard capture uses the active Hyprland layout via libxkbcommon. Device errors, input timeout, desktop lock/inactivity or bridge exit release grabs. Ctrl+Alt+Escape is an emergency release. Rapid mode clicks retain requested mode while waiting for Bluetooth acknowledgement.

The permanent keyboard-access rule is installed on mero-machine and user-confirmed. Verified uaccess tags and user ACLs on both CASUE interfaces. No per-use sudo is needed. Setup for another machine, optional keyboard IDs and stable by-id device configuration: [INSTALLATION.md](INSTALLATION.md). Details: [MERO_TERMINAL.md](MERO_TERMINAL.md).

Hardware evidence: native screenshot488/489 showed compact fastfetch; PC→Foston→PC input printed PC_KEYBOARD_OK once; portrait touch helper491 typed pwd and returned /home/zacmero. Managed tmux pane PID1577082 survived bridge restarts. Latest mode test532 observed0→1→0, both actual keyboard event handles open in the bridge during PC mode, and zero forwarding windows. Raw physical key translation tests exercise installed libxkbcommon (including Shift/Ctrl); a human key press after the seamless change remains a useful confirmation. Terminal ASCII cells use an embedded licensed bitmap because all tested OEM font names fell back to proportional Tahoma. Compact fastfetch now matches the user’s ArchMerOS branding and displays user/host, kernel, Bash, Hyprland, uptime and RAM; the live session was refreshed. Screenshot533/get534 verified restored touch keys and readable text.

Other apps updated with keyboard controls and exact readbacks: Ambient477/478 (4b6e76ab5b8a9059ac1fc041ad7a97ce7a23f5e2b0bdeb050a682c32c5bb68a9), Backlight480/481 (56f66dd6a4d370c960944c67bd005ce3868f6305ac4963bea995653864a3372c), Music483/484 (ca778a27cf33177193cc780623103d0bb4a1c7e9ec58b87b69c48c49460bdf74). Earlier Ambient/photo changes remain intact. Current terminal milestone changes are uncommitted. Full make check, native builds, shell syntax, udev verification and whitespace checks passed.

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

## Preservation complete — 2026-11-07
100 installed device files downloaded; final signed status895 agent_ready.
Private enriched manifest1681files, archive1682members. Member/source hashes,
GnuPG decryption and second-physical-drive/repo copies all verified.
Encrypted bundle: recovery-private/20261107-foston-preservation-v2.tar.gz.gpg
(explicitly Git-ignored), mirror /mnt/windows-ssd/Mero-Recovery/Foston/.
Encrypted SHA256 0d9c07f34604fe569dc23c01894f9efdd3514081f2ab1cb14d823d047c6d0f5e.
Read docs/RECOVERY.md before any recovery. No apps/startup/firmware modified.
Remaining user action: independently save the passphrase file at
~/.config/mero/foston-recovery/20261107-archive-passphrase. Do not print it in
chat or Git. No full firmware image, boot restoration test or off-machine copy.
Do not restore old host counters/device journals onto newer state.

Final validation: make, make check and git diff --check passed. Repo bundle
is ignored; second-drive ciphertext and GnuPG decryption verified. Current
revision is v2; earlier private archives retained. No restoration/fault test.
