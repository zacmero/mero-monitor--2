# Mero Terminal and PC keyboard

Mero Terminal runs a dedicated Bash session on mero-machine and displays its text on the Foston over the authenticated Bluetooth companion link. Open it from **01 – COMPANION LABS**. It is also available in **02 – DEFAULT APP**, using new stable ID17; earlier apps and startup IDs are preserved. The previous optional `mero-terminal.exe` entry is retained. This app is `mero-pc-terminal.exe`.

The dedicated tmux server is `mero-foston`, session `foston`. It sources normal Bash setup/aliases, suppresses the full-size startup fetch, prints a compact fastfetch with a three-line logo and uses a small prompt. It does not attach to or type into an existing desktop terminal. A separate user unit, `mero-foston-terminal.service`, owns the session so bridge restarts do not kill it. Reconnecting Bluetooth restores the screen. Closing the Foston app returns to Mero Shell while leaving the PC session running. Exiting Bash ends that session; CONNECT starts a new one.

## Touch controls

- **SHELL** returns home.
- **SHOW KB / HIDE KB** explicitly shows/hides the touch keyboard and resizes the PC terminal.
- **SCROLL** moves backward through up to1,000 lines of history. **LIVE** moves toward live output.
- **PC KEYS** engages physical forwarding inside the existing bridge and hides the touch keyboard. It becomes **PC OFF**; tap it to release forwarding while leaving the touch keyboard hidden. No desktop window opens.
- **SHIFT**, **SYM** and **CTRL** change the next keys. CTRL then C sends Ctrl+C. SYM switches punctuation and changes UP/DOWN to LEFT/RIGHT. Enter, Backspace, Escape and Tab have dedicated keys.

Orientation follows the main shell's saved portrait/landscape setting. The keyboard reduces visible rows; hide it to read more output. Input queues are bounded; old unsent input expires after15 seconds. A disconnect does not automatically replay uncertain input. Check the visible PC prompt after an interrupted command.

## Physical PC keyboard

Press **Super+Alt+Insert** on the PC to bring the Foston terminal to the front and enable keyboard forwarding. The dedicated Bash session is started or reattached as needed. Release the shortcut keys; capture starts only after they are up and the device confirms the terminal is foreground. Press the same shortcut again while captured to release the keyboard locally. The shortcut is handled inside evdev capture as well as Hyprland, so release still works while Hyprland cannot see grabbed keys. No desktop window opens.

The touch keyboard starts hidden and remains hidden when PC mode ends or fails. Use **SHOW KB** only when needed, then **HIDE KB** to recover the full terminal view. **PC KEYS / PC OFF** remains available on the Foston. Manual PC mode can control the other supported apps below; shortcut mode targets the terminal specifically and releases capture if fresh foreground confirmation is lost.

The bridge uses the current Hyprland keyboard layout through the installed libxkbcommon. A lost input acknowledgement or device error releases forwarding instead of retrying uncertain commands. Ctrl+Alt+Escape remains an emergency release. Physical keyboard access needs a one-time Linux device permission setup. Permanent setup: `sudo bash scripts/install_foston_keyboard_access.sh` once from this repository. It installs the validated70-mero-foston-keyboard.rules for exact USB vendor2a7a/product9005, keyboard interfaces only, and uses logind uaccess for the active seat user. This survives reboot/reconnect and requires no sudo during normal use. Existing rule versions are backed up before replacement.

Terminal supports printable text, Enter, Tab, Ctrl+C and arrows. The PC input travels to the Foston and then through the same terminal path back to the PC session. A signed `queued` acknowledgement means the device accepted input into its message queue, not that the shell command finished. Input goes to supported custom apps only; vendor/system windows are rejected. Normal PC keyboard behavior resumes when forwarding is off. Mouse input remains local to the PC.

Other supported apps:

| Target | Keys |
|---|---|
| Shell | Arrows/Tab select a highlighted button; Enter activates; Escape returns to its main page. |
| Ambient | Left/Right change photo; Space pauses; W toggles weather; M toggles controls; Escape returns to shell. |
| Backlight | Minus/plus adjust; 0 minimum; 9 maximum; R restores entry brightness; Escape returns to shell. |
| Music | Left/Right previous/next; Space play/pause; Escape returns to shell. |

Keyboard forwarding is explicit and limited to these custom apps. It does not implement a generic Windows CE Bluetooth HID driver or make every legacy/vendor app keyboard accessible.

## Transport and limits

Terminal events reuse signed live controls and durable PC event receipts. Input bytes are hex-encoded in bounded32-byte packets. Captures contain a bounded ASCII character grid, not screenshots; tmux handles the terminal emulation and interactive programs on the PC. ANSI/OSC sequences are stripped by plain tmux capture. Display colors use the native theme rather than reproducing application ANSI colors. Non-ASCII screen cells show `?`; text input supports BMP UTF-8 characters. Supplementary-plane PC keyboard input is rejected.

Portrait has32 columns and up to26 rows; landscape58 columns and14 rows. With keys open those become16 and4 rows. This is a small, Bluetooth-polled terminal; it is suitable for commands and modest interactive tools, with visible input delay. It is not a video-rate desktop mirror.

The bridge captures the physical keyboard only in explicit PC mode. A mode0600 Unix socket under XDG_RUNTIME_DIR remains available for diagnostic checks. Each request has an unpredictable ID and signed device acknowledgement. The agent checks the exact foreground class and journals intent before delivery, preserving duplicate suppression across agent restarts. Unknown delivery outcomes must not be retried automatically. The existing socket-write/PDU bounds and keyed upgrade path remain unchanged.

Terminal output and pending input stay in RAM on the Foston. The PC owns bounded tmux history. No terminal transcript is exported into the Bluetooth FTP share; only authenticated snapshots and acknowledgements use that share. Normal shell history behavior follows the user's Bash configuration. These commands run with the normal mero-machine user's privileges.

Build: `rtk make terminal`. Checks: `rtk proxy python3 scripts/check_terminal.py`, plus `rtk make check`. Hardware installation evidence is recorded in NEXT_SESSION/progress after deployment.

The OEM maps all tested font names to proportional Tahoma. Terminal cells therefore use an embedded8×14 ASCII bitmap derived from DejaVu Sans Mono; attribution and redistribution terms are in wince/terminal/FONT_LICENSE.txt. No font installation or system font change is needed.

The compact greeting follows the custom ArchMerOS branding (ArchMerOS x86_64) and shows the user/host, kernel, Bash, Hyprland, uptime and RAM. It is refreshed in the live session without changing the machine OS identity or normal fastfetch configuration. Non-ASCII output cells and full dead-key composition remain limitations of this first terminal build.

## Input and refresh correction

The CASUE board exposes two interfaces advertising the same full keyboard. Capture still grabs both so keys do not leak to Hyprland, but selects one actual typing source per engagement and ignores duplicate DOWN reports. True held-key repeat is retained. The touch app accepts one DOWN per UP boundary and highlights a pressed key immediately; its footer distinguishes queued input from an outstanding send.

During active terminal demand, host publication targets150ms, the native shared-bus reader polls every 50 ms and the agent reduces its deliberate pause to25ms. These are scheduling intervals, **not a promised frame rate**. Signed live hints skip idle job-file reads; actual job contents still undergo their original verification. Timed polling resumes when hints are missing or stale. Slow music queries run separately from terminal updates. Input is executed before capturing the outgoing frame. The agent uploads pending input before fetching the next screen and suppresses redundant uploads while awaiting acknowledgement. Foreground presence in the signed capability lease returns to the normal background cadence when the app is no longer active, without using the input mailbox for heartbeats.

Live snapshots can use zlib after an authenticated per-bridge capability exchange. The agent verifies the envelope HMAC before bounded inflation and checksum validation. A short capability lease restores plain snapshots for an older/rolled-back agent. Decoded state remains below4096 bytes; socket writes stay at 256 bytes, control PUT PDUs at 256 bytes and bulk PUT PDUs at 512 bytes. New status counters report actual GET, event-send and accepted-frame timing so scheduling targets are not confused with device performance.

The optional native `mero-terminal-latency.exe` waits10 seconds after launch, prints a unique harmless marker and measures application input to reception of its authenticated output frame. The delay isolates the agent's durable launch-receipt writes; do not run other agent jobs during the measurement. Build with `rtk make -C wince/probe mero-terminal-latency.exe`; report is SDMMC/MERO/terminal-latency.txt. Copy each result before another run. This measures frame reception rather than final paint. Earlier immediate-launch samples included SD journaling and cannot be used as a typing benchmark. Installed measurements and hashes are recorded in NEXT_SESSION/progress.

## Eliminating staging stalls

Fresh isolated baselines reached 6–7 seconds. First-attempt diagnostics found `ERROR_DISK_FULL` (112) while staging a live input file in the CE object store, before any Bluetooth packet was sent. The error branch then disconnected a healthy link. Retrying locally reduced the delay but still relied on the failing staging operation.

Transient signed input and codec messages now travel directly from RAM through the existing bounded OBEX sender. Pre-transfer failures keep the link; an attempted network transfer that fails still reconnects. Durable agent/deployment journals, keyboard delivery intent and PC execution receipts retain their flush and verification requirements.

When the terminal is foreground, a negotiated `live-terminal.txt` snapshot carries terminal text, cinema state and orientation with the same signed ACK, keyboard, job and codec fields. The normal app feed remains available. Missing compact files or frames that stop advancing fall back to the generic feed, including older-host rollback. Weather/music presence flags are normally zero during compact terminal reception.

The retained combination (RAM sender, compact snapshots, original terminal UI) received output in 928, 798 and 926 ms, with one output each and a successful first upload. UI timer/posted-flush experiments showed no measured improvement and were removed. These are small-sample input-to-received-frame observations, not final-paint timings or a frame-rate guarantee. See NEXT_SESSION for exact installed hashes, receipts and rollback references.

## Shortcut installation

ArchMerOS tracks the binding in the active `config/hypr/hyprland.lua` and legacy `config/hypr/hyprland.conf` and its windowless wrapper at `config/archmeros/scripts/archmeros-foston-terminal.sh`. The wrapper defaults to `$HOME/projects/mero-monitor-#2`; set `MERO_FOSTON_REPO` if the clone lives elsewhere. It calls `scripts/foston_keyboard.py toggle-terminal`. A failed or unresolved authenticated launch never arms capture or resets pending agent state. Existing permanent keyboard permissions are sufficient; no additional sudo step is needed.

The bridge polls physical input and acknowledgements every approximately 25 ms, independently of display publication. New keyboard packets/mode changes request an immediate snapshot. Active-seat and lock checks target 100 ms intervals while capture is active; errors release the keyboard. Scheduling intervals are not a promised end-to-end latency.

## Ctrl+Space and nested terminal clients

Ctrl+Space is the NUL control byte (`00`), not an ordinary space. The keyboard transport permits it only for the exact foreground Mero Terminal, with the same authenticated request, durable intent and duplicate checks. Other app targets reject a NUL-containing packet before posting any character. The touch keyboard's CTRL then Space (also CTRL+@/2) emits the same byte.

The user's Herdr configuration uses Ctrl+Space followed by D to detach the client and return to Bash, preserving its sessions. Super+Space is the PC launcher, not this prefix. In PC mode, release Ctrl+Space before pressing D; use Super+Alt+Insert to return the keyboard to the PC afterward.
