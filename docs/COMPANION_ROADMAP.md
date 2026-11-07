# Companion Labs: useful things to build

## Current priority — 2026-11-06

Ambient Home is the next priority. Additional PC command controllers and agent-monitoring dashboards are reference ideas only: the user already has better tools for both. Preserve those ideas below, but do not prioritize their implementation.

- Stream images from user-selected PC folders over Bluetooth. The PC enumerates and resizes images; the Foston retains bounded current-frame data in RAM, without copying the photo library to SD or resident flash. Preserve existing photos and folder settings.
- Adapt layout, fit/crop, slideshow timing and subtle motion to the selected folder and image orientation. Include folder selection, a clear connection/freshness indicator, and an easy exit to Mero Shell.
- Combine the streamed gallery with fresh weather and the existing frequency-matrix ambient visuals. Keep optional music information lightweight.
- Weather must distinguish data-fetch age from link age, retry failed fetches, and label stale data honestly.

Status: Ambient Home is now implemented and deployed as the PC-folder streaming app, alongside the preserved Local Gallery. It combines orientation-aware RAM-only photos, folder selection, slideshow controls and live weather. Native hardware checks verified selection, pause, next while paused, resume and shell exit. Weather refreshes every ten minutes and shows fetch age. See [Ambient Home](AMBIENT_HOME.md) for configuration. Frequency Matrix remains a separate existing app; incorporating it into Home and adding optional music information remain future work.


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

Local utility delivered: Backlight (Companion Labs first page), enabling direct minimum/maximum comparisons and restore without vendor UI. Physical brightness efficacy is still to be confirmed by the user.

## Approved next app — Mero Terminal (2026-11-07)

User approved persistent PC terminal text over Bluetooth, compact fastfetch, touch keyboard and explicit PC keyboard forwarding into custom Foston apps. Implementation/usage: [MERO_TERMINAL.md](MERO_TERMINAL.md). Keep firmware work behind this app priority. Existing optional terminal and saved IDs are preserved.
