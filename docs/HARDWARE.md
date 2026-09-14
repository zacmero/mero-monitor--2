# Foston FS-460BT Hardware & Platform Reference

This document records all confirmed hardware, firmware, and peripheral specifications for the Foston FS-460BT GPS unit repurposed as `mero-monitor-#2`.

---

## 1. Core Hardware Specifications

| Property | Value / Specification | Notes |
| :--- | :--- | :--- |
| **Device Model** | Foston FS-460BT | Generic PNA / WinCE GPS class |
| **SoC / Processor** | MediaTek MT3351 (ARM926EJ-S) | Confirmed via `MT3351Calibration.exe` |
| **Operating System** | Microsoft Windows CE 5.0 (Build 1400) | Verified via `mero-probe` on hardware |
| **Processor Architecture** | ARMv4 / ARMv4I / ARMv5TEJ | 32-bit Little-Endian PE32 binaries |
| **Display Resolution** | 480 × 272 pixels | Verified 16 bpp RGB565 |
| **Touchscreen** | Resistive single-touch | Handled via `Touch` driver & `TouchCalibrate.exe` |
| **Color Depth / Pixel Format** | 16-bit RGB565 | Confirmed via hardware probe & Sygic config |
| **RAM** | 53 MB usable (64 MB physical) | ~38 MB free at baseline (28% load) |
| **Internal Flash / ROM** | 128 MB | NAND flash (`MSFLASH` driver) |
| **External Storage** | 8 GB SD / MicroSD card | Mounted in WinCE as `\SDMMC` |

---

## 2. Ports & Peripherals (Discovered via Architecture Dump)

### GPS Module
- **Interface**: Internal UART (`Uart1` / `GPS` / `GPS2` drivers)
- **Device Port**: `COM1:`
- **Baud Rate**: `9600` baud
- **Data Protocol**: NMEA-0183 standard sentences (`$GPGGA`, `$GPRMC`, `$GPGSV`, etc.)

### Bluetooth & Audio
- **Bluetooth Subsystem**: `HciExt` driver + `btpower.exe` (launched on boot at `Launch80`)
- **A2DP Audio**: Native `BtA2dpSnd` driver present in Windows CE
- **FM Transmitter**: Native `fmc` driver present in `HKLM\Drivers\BuiltIn\fmc`

### USB Controller & Modes
The device supports two switchable USB controller profiles (`UsbFn` driver):
1. **Mass Storage Mode**: VID:PID `045e:ffff` (`Microsoft Windows CE Mass Storage`)
2. **ActiveSync Mode**: VID:PID `045e:00ce` (`Microsoft Generic PPC Flash device`) -> Linux `ipaq` -> `/dev/ttyUSB0`

---

## 3. Windows CE Boot Sequence (`HKLM\init`)

The confirmed boot sequence recorded from hardware dump:
* `Launch20` = `device.exe` (Device driver manager)
* `Launch30` = `gwes.exe` (Graphics, Windowing, and Events Subsystem)
* `Launch50` = `Launch.exe` (**The Vendor PNA Shell!**)
* `Launch60` = `services.exe` (Service manager)
* `Launch80` = `btpower.exe` (Bluetooth power initializer)

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
