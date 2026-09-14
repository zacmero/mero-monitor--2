# mero-monitor-#2

Repurposing a Foston FS-460BT Windows CE GPS device into a dedicated `Mero Thought Terminal`.

---

## Overview

`mero-monitor-#2` runs custom, native ARM Windows CE software directly from an SD card (`\SDMMC\MERO\`). It preserves the original vendor OS, bootloader, and device drivers, using Windows CE as a reliable hardware abstraction layer for the screen, resistive touchscreen digitizer, internal GPS UART, and USB connectivity.

---

## Hardware Profile

- **Display**: 480 × 272 pixels, 16-bit RGB565 resistive touchscreen
- **Memory**: 64 MB RAM, 128 MB ROM
- **Storage**: 8 GB SD Card mounted at `\SDMMC`
- **Target Binary**: PE32 Windows CE ARMv4 / ARMv4I Little-Endian
- **GPS UART**: `COM1:` at 9600 baud (NMEA-0183)
- **USB Host Connection**: ActiveSync mode (`045e:00ce`) binding to `/dev/ttyUSB0` via the Linux `ipaq` kernel driver

Detailed hardware specs and findings are documented in [`docs/HARDWARE.md`](file:///home/zacmero/projects/mero-monitor-%232/docs/HARDWARE.md).

---

## Project Structure

- [`docs/`](file:///home/zacmero/projects/mero-monitor-%232/docs/): Hardware specs, platform architecture, and handoff notes.
- [`wince/probe/`](file:///home/zacmero/projects/mero-monitor-%232/wince/probe/): First-stage native diagnostic tool (`mero-probe.exe`).
- [`wince/thought-terminal/`](file:///home/zacmero/projects/mero-monitor-%232/wince/thought-terminal/): Native Mero Thought Terminal client.
- [`host/bridge/`](file:///home/zacmero/projects/mero-monitor-%232/host/bridge/): Host-side utilities for USB/ActiveSync and serial communications.
- [`assets/`](file:///home/zacmero/projects/mero-monitor-%232/assets/): Bitmaps, palettes, and icon assets.

---

## Quick Start: Building `mero-probe`

The project utilizes a modern containerized CeGCC (GCC 9.3.0 for ARM-MinGW32CE) environment:

```bash
make -C wince/probe
```

Copy the resulting binary to `\SDMMC\MERO\mero-probe.exe` on the SD card, update the device's navigation executable path in settings, and launch.
