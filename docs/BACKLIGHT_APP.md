# Backlight controls

Open **Shell → 01 – Companion Labs → Backlight** (first page). It runs locally on the Foston; Bluetooth is needed to install it, not to use its buttons. It follows the shell orientation preference and participates in cinema blanking.

- **− DIMMER / + BRIGHTER** changes one vendor level at a time, bounded to 0–10.
- **MINIMUM / MAXIMUM** selects the ends of that range.
- **RESTORE** returns to the level read when the app opened.
- **SHELL >** exits and keeps the current level for the powered session.

Compare MAXIMUM with MINIMUM while looking at the white/gray patches. LEVEL is the driver-reported setting, not a measurement of emitted light. Level 0 is the vendor minimum and can still glow. If the level changes but the physical brightness does not, do not treat the display as dimmed. No values outside the OEM range are offered.

The app uses the same BKL1 brightness read/write implementation as the direct script and cinema dimming, with driver readback after each adjustment. The value at entry is retained in RAM for RESTORE. Existing Display Tuner and other apps were preserved; the shell startup default was not changed.

Build: `rtk make backlight`. Unit checks extract the actual C button actions and verify min/max bounds, increments, restore and read/write failure handling.

The user confirmed that the manual brightness controls work and that the vendor minimum is still physically bright. Full physical backlight-off has not been verified.
