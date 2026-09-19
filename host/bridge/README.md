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
