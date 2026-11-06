# Companion Labs: useful things to build

The working link is two-way Bluetooth FTP initiated by the Foston. The PC
does network access and heavier work; the WinCE device draws locally, accepts
touches and keeps usable cached/offline views. USB is optional. No working
battery means every portable idea needs external power.

## First: improve the apps already here

- **Weather Window:** fresh PC conditions and three-day forecast, locally animated
  clouds/rain/sun, fetched age and a visible offline/cache state. Keep provider
  observation time separate from the time the PC fetched it.
- **YouTube Music:** real title/artist, playback state, elapsed/duration, local
  progress animation, previous/play/pause/next and seeking when the browser
  supports it. Select only music.youtube.com. Reset on video ID changes, freeze
  stale progress, disable unavailable controls and acknowledge each press.
  Include the current thumbnail in a single bounded RAM slot; clear the previous
  cover on track changes and keep artwork files off the SD card.
- **Companion Labs:** one app catalogue, direct Weather/FM/GPS entries, a saved
  startup default, cancelable startup countdown, and a consistent exit to shell.
  Returning home must never relaunch the default immediately.
- **Adaptive Gallery:** retain folder-specific fit/fill/motion profiles. Next add
  PC-supplied playlists, folder previews, orientation-aware layouts and optional
  timed/sunrise/cinema themes. Preserve original images and settings.
- **Frequency Matrix:** retain the ambient synthesizer and its actual PCM-driven
  wave. Later send real PC audio energy/frequency bands for an honest spectrum;
  animated random bars must be labelled decoration. OEM FM transmission control
  remains unverified; do not revive guessed hardware IOCTLs.
- **Camera Monitor:** make connection age and reconnect behavior explicit; allow
  small PC-resized frame updates if Bluetooth bandwidth proves sufficient.

## Next useful applications

| Idea | Practical use | What it needs |
|---|---|---|
| Control | Six large configurable buttons: cinema, media, volume, focus timer, screenshot, a chosen desktop action. | A small, explicit PC action allowlist; acknowledgements and button state. |
| Activity | Jobs running/completed/failed, build results, elapsed time and connection health. | PC relay first. Most agents run on a VM: add a VM-side summary feed through the existing authorized SSH setup later. No VM credentials on the Foston. |
| Ambient | Adaptive gallery, fresh weather and the frequency visuals in one gentle rotation. | Local playlists and transitions, cinema blank/wake, per-folder settings. |
| Focus | Start/stop a work timer on either device, see remaining time and an understated end alert. | Persistent timer deadline on PC; local countdown between updates. |
| Inbox | A few useful notifications, queued while offline, tap to acknowledge or dismiss. | Explicit source selection and quiet hours; no unfiltered desktop notification firehose. |
| Notes / file drop | Send a short note, checklist, image or file to the Foston; return a selected file to the PC. | Existing verified transfer plus a small inbox UI and safe file destinations. |
| Outdoor GPS logger | Real track, distance and GPX export over Bluetooth after a walk/ride. | A real GPS fix, external power and buffered SD recording. Indoors shows waiting rather than invented movement. |
| Device workbench | Memory/link health, logs, app versions, screenshots and a backed-up app update/rollback view. | Narrow agent diagnostics and authenticated operations; no firmware-flashing shortcuts. |
| Desk status | Clock, next calendar/task item and PC connection state when music is stopped. | Optional chosen data sources; a useful offline clock. |

## Reliability rules

- Low-rate signed state snapshots; local animation. Telemetry lives in shared
  RAM on CE, not a new SD file/backup for each animation frame.
- Touch actions are allowlisted, bound to the displayed track and acknowledged.
  A stale/repeated command must not unexpectedly skip another song.
- Keep the original recovery shell and factory files. Deploy custom apps with
  verified backups; validate actual device behavior after builds.
- Default app selection is shell configuration. Reuse the existing boot chain;
  do not rewrite Launch50 or guess OEM boot settings to implement an app picker.
- Cinema is reversible software blanking with a disconnect recovery timeout.
  Actual hardware suspend/backlight control needs separate device evidence.
- Do not claim automatic power-on from a software setting. The hardware must
  already boot when USB power returns; cold-power behavior requires a physical test.
