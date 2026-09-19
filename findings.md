# Findings

- Firefox MPRIS currently supplies title, artist, play state, position, and duration correctly.
- USB `045e:00ce` binds to Linux `ipaq` as `/dev/ttyUSB0`.
- Opening `/dev/ttyUSB0` is not an ActiveSync session; the current host log reports only a successful file descriptor open.
- `mero-media-ctrl.exe` currently kills ActiveSync processes and probes guessed COM ports, preventing the documented PPP path.
- The repository already has the required `CLIENT`/`CLIENTSERVER` + `pppd` launcher and documents custom TCP over PPP.
- Simultaneous Linux/WinCE FAT access cannot be made reliably coherent with `os.sync`, rename, or read-cache flush guesses.
- The image/gallery visualizer does not use ActiveSync networking. It reads `\SDMMC\Stream` files prepared by `scripts/mero_stream_bridge.py`.
- Live PPP failed before IP negotiation: `chat` sent `CLIENT`, waited for `CLIENTSERVER`, and timed out on every attempt.
- SynCE 0.15.2 `synce-serial-chat.c` proves the correct direction: read six-byte `CLIENT` from the device, then write twelve-byte `CLIENTSERVER` from the host. The repo's PPP script had this reversed.
- Corrected PPP negotiated `192.168.55.101`/`192.168.55.100`, but firewalld blocked TCP port 5000 until `ppp0` was assigned to the trusted zone.
