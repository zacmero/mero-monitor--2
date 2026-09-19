# Progress

- Inspected live USB, mounts, service logs, MPRIS state, host bridge, WinCE receiver, and PPP launcher.
- Confirmed the existing WinCE source builds, while the live transport remains unverified and nonfunctional.
- Selected PPP/TCP using the existing line protocol as the minimum reliable design.
- Replaced host raw serial sends with a reconnecting TCP listener on `192.168.55.101:5000`.
- Added a nonblocking WinSock client and switched the WinCE polling timer to it.
- Cross-built the 73 KB WinCE executable and confirmed the deployed SD copy has the same SHA-256 hash.
- Python syntax, host TCP loopback exchange, build, and whitespace checks passed.
- Started transient root service `mero-ppp.service`; it is waiting for `/dev/ttyUSB0` while the device remains in Mass Storage mode.
- PPP handshake attempts failed before creating `ppp0`; no TCP connection was possible.
- Changed storage sync to fixed-file overwrite/open/read behavior and made `[LIVE SYNC]` require changing file content within three seconds.
- Removed storage freshness from the live badge and corrected the PPP chat sequence to expect `CLIENT` then send `CLIENTSERVER` without a trailing carriage return.
- Verified `ppp0`, bidirectional ping, bridge listener, and an established WinCE TCP client at `192.168.55.100:1034 -> 192.168.55.101:5000`.
