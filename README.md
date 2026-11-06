# mero-monitor-#2

Repurposing a Foston FS-460BT Windows CE GPS device into a dedicated `Mero Thought Terminal`.

Current repair notes, live USB setup, cinema standby, gallery profiles, deployment backups, and hardware verification limits: [docs/REPAIR.md](docs/REPAIR.md).

## Verified milestone: Bluetooth file transfer

On November 5, 2026, the Foston downloaded and ran `mero-bt.exe` over Bluetooth. The custom app then listed the PC share, downloaded a proof file, and uploaded its 42-byte test payload. The PC verified the upload byte for byte. USB mounting was not needed.

- Keep the device in **ActiveSync / PC Link**, with USB supplying power. Mass Storage reserves the SD card and can make Bluetooth downloads fail as read-only even when the PC cannot enumerate USB.
- The initial download uses the native Bluetooth File Manager. Its destination is `\SDMMC\BT\`. Set the navigation executable path to the downloaded `mero-bt.exe` to launch the custom client.
- The custom client provides **CONNECT / LIST**, **DOWNLOAD FILE**, and **UPLOAD TEST**. Downloads go to uniquely named files under `\SDMMC\BT\Downloads\`; they do not replace installed apps.
- Shell 0.2.12 also finds the client in `\SDMMC\BT\` through **System Tools → Bluetooth → Bluetooth Files**.
- The current client targets the already paired mero-machine adapter, `8C:86:DD:83:27:70`. It supports a flat directory of ASCII filenames; new-device pairing and nested browsing remain future work.

The PC serves `~/.local/share/mero-foston-bluetooth/` through `mero-bt-ftp.service`. See the [host setup](host/bridge/README.md) and [verified setup notes](docs/REPAIR.md). Resident flash and boot configuration are preserved.

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

Detailed hardware specs and findings are documented in [`docs/HARDWARE.md`](docs/HARDWARE.md).

---

## Project Structure

- [`docs/`](docs/): Hardware specs, platform architecture, and handoff notes.
- [`wince/probe/`](wince/probe/): First-stage native diagnostic tool (`mero-probe.exe`).
- [`wince/bluetooth/`](wince/bluetooth/): Native OBEX FTP file-transfer client.
- [`wince/thought-terminal/`](wince/thought-terminal/): Native Mero Thought Terminal client.
- [`host/bridge/`](host/bridge/): Host-side Bluetooth FTP, weather, cinema, and USB/ActiveSync services.
- [`assets/`](assets/): Bitmaps, palettes, and icon assets.

---

## Quick Start: Building `mero-probe`

The project utilizes a modern containerized CeGCC (GCC 9.3.0 for ARM-MinGW32CE) environment:

```bash
rtk make
rtk make check
```

Copy the resulting binary to `\SDMMC\MERO\mero-probe.exe` on the SD card, update the device's navigation executable path in settings, and launch.
