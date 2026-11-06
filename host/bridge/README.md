# Host Bridge Architecture & ActiveSync / RAPI

Investigation and implementation details for host communication between Linux and the Foston FS-460BT Windows CE device.

---

## 1. Confirmed Hardware Layer

- **Device Mode**: ActiveSync (USB `045e:00ce`, `Microsoft Corp. Generic PPC Flash device`)
- **Kernel Module**: `ipaq` (PocketPC PDA converter)
- **Host Device Node**: `/dev/ttyUSB0` (permissions: `rw-rw----`, group: `uucp`)

---

## 2. ActiveSync Serial Protocol Details

Unlike Mass Storage mode (which is a block storage interface), ActiveSync over USB on Windows CE is **not** a raw ASCII serial console.

It operates as a layered protocol:

```text
Host Linux Application
    │
    ▼
RAPI (Remote API) / ActiveSync Client (TCP Port 990 / 5678 / 5679)
    │
    ▼
PPP (Point-to-Point Protocol) Layer (192.168.55.101 <-> 192.168.55.100)
    │
    ▼
Serial Framing / Handshake ('CLIENT' / 'CLIENTSERVER')
    │
    ▼
Linux /dev/ttyUSB0 (ipaq driver)
    │
    ▼ [USB D+/D-]
Windows CE USB Client Controller
```

### The Connection Sequence:
1. When connected, the WinCE device listens on the serial port.
2. The host or device sends the sync trigger string (`CLIENT` or `CLIENTSERVER`) over the serial connection.
3. Once synchronized, standard PPP negotiation starts (`pppd /dev/ttyUSB0 115200 ... local 192.168.55.101:192.168.55.100`).
4. Once IP connectivity is established, standard TCP sockets can be opened between the host and the WinCE device:
   - Port 990 / 5678: RAPI (Remote API for file copy, remote process execution, registry queries).
   - Custom TCP port (e.g. 5000): For streaming Suzy / Mero Thought artifacts directly into `mero-terminal.exe`.

---

## 3. Tooling Options on Modern Linux

1. **Native PPP daemon (`pppd`)**:
   Standard system daemon already installed at `/usr/bin/pppd`. Can establish the IP bridge directly.

2. **SynCE / CeSync / RAPI**:
   Open-source Linux implementation of the Microsoft ActiveSync suite (`synce-serial`, `rapi`).

3. **Custom Socket Bridge**:
   Once PPP is up, a simple TCP server running on the WinCE device (via standard Winsock `ws2.dll`) or on the host allows bidirectional streaming of text artifacts, thoughts, and control commands.

## Live Media Sync

`scripts/mero_bridge.py` listens on `192.168.55.101:5000`. `mero-media-ctrl.exe`
connects to that address and reconnects automatically. The bridge sends newline-delimited
`MERO:NOW:` and `MERO:WX:` records through the TCP connection.

1. Switch the device to Mass Storage mode and wait for the updated
   `MERO/mero-media-ctrl.exe` to be copied.
2. Eject or unmount the SD card cleanly.
3. Switch the device to ActiveSync mode.
4. Start PPP with `sudo ./host/bridge/start_internet_ppp.sh`.
5. Start `mero-media-ctrl.exe` on the device.
6. Verify that the app header changes from `[SD CACHE]` to `[LIVE SYNC]`.

Do not use the SD card as a live mailbox while WinCE has it mounted. Mass Storage is
only the deployment path; PPP/TCP carries live state.
# Current bridge

For the current weather/cinema runtime, persistent PPP service, and device-specific recovery constraints, see [repair notes](../../docs/REPAIR.md). Older transport examples below are retained as reference; the current apps never terminate ActiveSync or use raw serial writes for live state.

## Bluetooth FTP staging service

`mero-bt-ftp.service` serves only `~/.local/share/mero-foston-bluetooth/`. The verified Foston setup uses this service for its initial native-client download and subsequent custom-client listing, download, and upload. Keep the Foston in ActiveSync/PC Link so its SD card is writable locally; this link does not require a working USB connection.

The checked-in unit describes the tested mero-machine runtime: BlueZ 5.72 `obexd` and the libraries reported by its `ldd` are copied from the laptop under `~/.local/share/mero/obex-compat/`, retaining their `usr/libexec/bluetooth/` and `lib/x86_64-linux-gnu/` paths. These binaries are not vendored in Git, and installing this unit alone does not install its runtime. It loads only the `ftp`, `bluetooth`, and `filesystem` plugins, without allowing symlinks outside the staging root. System BlueZ packages and kernel drivers are unchanged.

BlueZ 5.87 `obexd` refused to register its transport while mero-machine was at the greeter without an active local desktop session. The private 5.72 runtime avoids that startup dependency and is enabled as a user service with lingering on this host. Other hosts may use their packaged `obexd`; update the unit's `ExecStart` and library path to match and verify that it advertises FTP UUID `1106`, including after reboot.

```bash
rtk systemctl --user status mero-bt-ftp.service
rtk journalctl --user -u mero-bt-ftp.service -n 30
rtk systemctl --user stop mero-bt-ftp.service
```

Shared ARM executables are build outputs, not automatic updates. The custom client stages downloads on SD and does not overwrite the installed shell or recovery binaries. Initial pairing still uses the device's native Bluetooth controls.
