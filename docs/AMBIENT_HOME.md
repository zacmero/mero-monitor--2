# Ambient Home

Open **01 – Companion Labs → Ambient Home**. It is also available in **02 – Default App**. Installing it does not change an existing startup preference. The previous gallery remains available as **Local Gallery**.

The Foston pulls resized still images from selected PC folders over the existing authenticated Bluetooth link. Originals remain on the PC. The device retains a single verified 240×136 RGB565 frame in a separate RAM mapping plus its display buffer; it does not download a photo library to SD or resident flash. Each transfer is checked for exact size and SHA-256 against signed metadata. Frames are fetched only while Ambient Home is requesting them in the foreground, and the cache is cleared after app exit.

Use **Folders** to choose a configured PC folder. **Previous**, **Next**, and **Pause/Resume** control the slideshow; Next also works while paused. The slideshow waits for a displayed-frame acknowledgement before starting its interval. The default full-screen view shows only the photo and weather widget. Tap anywhere to reveal the menu; **FULL SCREEN** hides it again. **HIDE/SHOW WEATHER** toggles the widget, and **Shell >** returns home. Folder and filename strips and loading captions are removed. The previous verified photo remains visible while the next photo transfers. Weather shows fetch age and explicitly labels stale conditions as cached. Photo framing adapts to orientation: wide images fill, while tall images retain their composition against a blurred background. Motion and weather icons animate locally.

## PC folder configuration

`~/.config/mero/ambient.json` contains up to eight folders, an interval in seconds (15–600), and the saved selection. On first use it selects Pictures and its immediate subfolders. Folder contents are read directly, without recursion; supported types are JPEG, PNG, WebP and BMP. Symlink files are skipped. Images are limited to 32 MB compressed and 24 megapixels; unavailable images are skipped. Empty/unavailable folders show a message.

Example:

```json
{
  "folders": [
    {"name": "Wallpapers", "path": "/home/zacmero/Pictures/bg"},
    {"name": "Pictures", "path": "/home/zacmero/Pictures"}
  ],
  "interval": 20,
  "selected": 0
}
```

After editing the configuration, run `rtk proxy systemctl --user restart mero-bridge.service`. Folder changes made on the Foston save the selection automatically. An active app resumes sending demand after a bridge restart. The PC retains at most two owned resized frame files in the private Bluetooth share; source pictures and unrelated files are preserved. This is a still-image slideshow, not video streaming.

## Orientation, cinema and photo throughput

The Ambient menu offers ROTATE for portrait/landscape; the main shell also has 06 – ORIENTATION. This rotates the entire Ambient layout and touch coordinates, with no toggle over the clean view. The choice is saved on the PC. Photos fill the viewport behind the optional weather overlay in either orientation. Existing legacy apps retain their own layouts.

Photo frames now use bounded JPEG encoding inside the existing content-addressed transfer, then decode to the same 240×136 RGB565 RAM slot. The agent validates the compressed payload hash and exact decoded dimensions; legacy raw frames remain supported. Socket writes stay bounded to 256 bytes and agent PUT frames to 512 bytes. Measured samples: raw 65,280-byte download 7,806 ms; JPEG 10,046-byte download 1,330 ms plus 21 ms decode. These were different slideshow frames, not a controlled same-image benchmark.

Cinema uses the existing ArchMerOS state feed. The active BKL1: driver successfully accepted Get/SetDevicePower D0→D4→D0 on hardware without dropping Bluetooth. The power-state API reports D4 but the user confirmed that the LCD still glows. It is now replaced by the exact OEM brightness control: BKL1 GET 0x22000c / SET 0x220008 with a four-byte value. Cinema sets the vendor minimum raw level 14 and restores the original raw level afterwards; a versioned SD restoration journal preserves that value across agent upgrades. Full physical backlight-off remains unverified. This leaves the CPU and Bluetooth awake, rather than attempting whole-device suspend.

Shell 0.3.5 also rotates its whole layout and touch coordinates; portrait is the saved default. Shell and Ambient menus use distinct amber, coral, mint, violet, cyan and rose accents, with stronger selected-item backgrounds. Super+Alt+S uses side-monitor standby rather than the cinema JSON file; the bridge follows DP-2 DPMS state as well as the cinema file, matching the current ArchMerOS shortcut.

Physical backlight correction (2026-11-07): the user still sees unchanged glow even when BKL1 reports D4 and when the exact OEM brightness control reads back 14. Neither response proves physical dimming/off. Black pixels are verified; physical dimming remains unresolved. The active BKL driver is Windows/buses.dll, whose ROM contents could not be retrieved through the supported read interface. Do not introduce guessed off/PWM IOCTLs or call whole-device suspend as a workaround.

## Direct minimum-brightness command

From the repository, run:

```sh
rtk proxy python3 scripts/foston_backlight_min.py
```

This launches the installed native `mero-backlight-min.exe` over authenticated Bluetooth, which reads the current OEM backlight level, requests raw 14 (vendor minimum), reads it back and **leaves it at minimum**. It does not blank the image, enter cinema, or restore after a timed test. The script verifies that the returned report belongs to the newly launched process. On-device report: `SDMMC/MERO/backlight-manual.txt`. The first live execution returned before=19, requested=14, after=14, ok=1. A correct readback is not proof of physical dimming; the user's visual confirmation is pending.

Provisioning for another keyed device: `rtk make -C wince/probe mero-backlight-min.exe`, then deploy that public helper to SDMMC/MERO through `foston_agent.py put`. No vendor UI, navigation-path change, Docker runtime on the Foston, or device reboot is needed.

## Full-screen photo and weather tap correction

All photos now fill the entire viewport in both orientations, using a centered crop regardless of source aspect ratio. There is no small fitted image over a blurred backdrop. Cropping can remove photo edges. Originals on the PC remain unchanged.

Tap the weather widget to hide it. Tap the same area again to restore it. Tapping the photo outside that area opens the controls. The weather hit area is disabled while the folder chooser covers it, so folder selection still works.
