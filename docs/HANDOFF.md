# Mero Monitor #2: Project Handoff & Architecture

Repurposing a Foston FS-460BT Windows CE GPS device into a dedicated `Mero Thought Terminal`.

---

## 1. Core Philosophy

1. **Do not flash ROM, modify bootloaders, or replace the vendor OS.**
   - The vendor Windows CE BSP provides stable, hardware-tested drivers for the screen, touch digitizer, power management, audio, and GPS UART.
   - We treat Windows CE as our native Hardware Abstraction Layer (HAL).

2. **Reversible SD-Card Deployment.**
   - All custom software runs directly from `\SDMMC\MERO\`.
   - The vendor launcher's configurable "Navigation Executable" setting is repurposed to launch our native binaries without overwriting original assets (`iGO8`, `Sygic`).

3. **Minimalism & Low Animation Burden.**
   - The resistive 480×272 display is modest in refresh rate and color accuracy.
   - UI aesthetics prioritize:
     - Pure black backgrounds (`#000000`)
     - Monospaced, high-contrast typography
     - Low CPU/memory overhead
     - Subtle status indicators and deliberate text pacing.

---

## 2. System Architecture

```text
Host Linux Environment (EndeavourOS)
    │
    │ ActiveSync / USB (/dev/ttyUSB0 via ipaq driver)
    ▼
Windows CE 5.0 / 6.0 Subsystem (Foston FS-460BT)
    │
    ├── Native Mero Applications (\SDMMC\MERO\*.exe)
    │     ├── Phase 1: mero-probe.exe (Diagnostics & verification)
    │     └── Phase 2: mero-terminal.exe (Thought Terminal)
    │
    └── Vendor Windows CE Drivers & Peripherals
          ├── Display & GDI / Framebuffer (480x272 RGB565)
          ├── Touch Screen Digitizer (WM_LBUTTONDOWN / Touch API)
          ├── GPS Receiver (COM1: @ 9600 baud NMEA)
          ├── ActiveSync / RAPI Communications
          └── Bluetooth / Audio / Storage (\SDMMC)
```

---

## 3. Development Priorities & Milestones

1. **Arbitrary Code Execution**: Build minimal WinCE PE32 ARMv4 executable with modern toolchain and execute via Navigation path.
2. **Display & Diagnostics**: Fullscreen 480×272 window, system metrics (OS version, RAM, resolution), visual confirmation.
3. **Touch & Interaction**: Event handling for resistive touch coordinates, clear touch feedback, and exit button.
4. **File I/O**: Diagnostic logging to `\SDMMC\MERO\probe.txt`.
5. **Serial / GPS**: Query and read NMEA stream from `COM1:` at 9600 baud.
6. **Host Communication**: ActiveSync / RAPI bridging over `/dev/ttyUSB0`.
7. **Mero Thought Terminal**: Live rendering of Suzy/Mero Protocol artifacts (memories, focus states, reflections).

---

## 4. Repository Structure

```text
mero-monitor-#2/
├── README.md
├── docs/
│   ├── HARDWARE.md
│   └── HANDOFF.md
├── wince/
│   ├── probe/              # Native WinCE probe source and build files
│   └── thought-terminal/   # Mero Thought Terminal client application
├── host/
│   └── bridge/             # Linux host utilities, ActiveSync/RAPI scripts
└── assets/                 # Graphics, fonts, reference dumps
```
