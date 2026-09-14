# Foston FS-460BT Hardware & Platform Reference

This document records all confirmed hardware, firmware, and peripheral specifications for the Foston FS-460BT GPS unit repurposed as `mero-monitor-#2`.

---

## 1. Core Hardware Specifications

| Property | Value / Specification | Notes |
| :--- | :--- | :--- |
| **Device Model** | Foston FS-460BT | Generic PNA / WinCE GPS class |
| **Operating System** | Microsoft Windows CE 5.0 (Build 1400) | Verified via `mero-probe` on hardware |
| **Processor Architecture** | ARMv4 / ARMv4I | 32-bit Little-Endian PE32 binaries |
| **Display Resolution** | 480 × 272 pixels | Verified 16 bpp RGB565 |
| **Touchscreen** | Resistive single-touch | Verified responsive via `mero-probe` |
| **Color Depth / Pixel Format** | 16-bit RGB565 | Confirmed via hardware probe & Sygic config |
| **RAM** | 53 MB usable (64 MB physical) | ~38 MB free at baseline (28% load) |
| **Internal Flash / ROM** | 128 MB | Reported by device system information |
| **External Storage** | 8 GB SD / MicroSD card | Mounted in WinCE as `\SDMMC` |

---

## 2. Ports & Peripherals

### GPS Module
- **Interface**: Internal UART
- **Device Port**: `COM1:`
- **Baud Rate**: `9600` baud
- **Data Protocol**: NMEA-0183 standard sentences (`$GPGGA`, `$GPRMC`, `$GPGSV`, etc.)

### USB Controller & Modes
The device supports two switchable USB controller profiles in the vendor settings:

1. **Mass Storage Mode**
   - USB VID:PID: `045e:ffff` (`Microsoft Windows CE Mass Storage`)
   - Function: Exposes `\SDMMC` as a standard USB Mass Storage block device to the host.

2. **ActiveSync Mode**
   - USB VID:PID: `045e:00ce` (`Microsoft Generic PPC Flash device`)
   - Linux Driver: Binds to kernel module `ipaq`
   - Linux Device Node: `/dev/ttyUSB0`
   - Kernel dmesg confirmation: `PocketPC PDA converter now attached to ttyUSB0`
   - Protocol: Windows CE ActiveSync / RAPI transport layer.

### Additional Features / Peripherals (To Explore)
- **Bluetooth**: Present on hardware (`-BT` suffix), RFCOMM/DUN profile support in WinCE.
- **Audio / Speaker**: Internal speaker and 3.5mm jack.
- **FM Transmitter / Radio**: Vendor PNA FM broadcast capability.
- **Analog TV**: Hardware TV tuner (vendor antenna/app present on some revisions).

---

## 3. Storage Layout & Path Conventions

| Path | Description | Access Mode |
| :--- | :--- | :--- |
| `\SDMMC` | Root of the removable 8 GB SD Card | Read/Write FAT32 |
| `\SDMMC\IGO8\iGO8.exe` | Original vendor navigation binary | **DO NOT OVERWRITE** |
| `\SDMMC\MERO\` | Target directory for custom Mero binaries | User application root |
| `\SDMMC\MERO\mero-probe.exe` | Initial hardware verification binary | Target entry point |
| `\SDMMC\MERO\probe.txt` | Diagnostic log file output | Written by `mero-probe` |

### Executable Launch Mechanism
The Foston UI provides a user-facing setting:
`Settings -> Navigation Path` (or `GPS Path`).
- Setting this to `\SDMMC\MERO\mero-probe.exe` executes arbitrary native WinCE ARM binaries directly upon pressing the "Navigation" button on the main vendor shell.
- This allows full native execution without patching ROM, altering bootloaders, or modifying flash memory.
