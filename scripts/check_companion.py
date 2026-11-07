#!/usr/bin/env python3
"""Focused offline checks: TCP fanout, snapshots, reconnect, cinema, USB identity."""
import importlib.util
import json
import os
import socket
import subprocess
import tempfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def check():
    bridge = module("bridge", ROOT / "scripts/mero_bridge.py")
    sync = module("sync", ROOT / "host/bridge/mero_sync.py")
    deploy = module("deploy", ROOT / "scripts/deploy_sd.py")
    bridge.STREAM_HOST, bridge.STREAM_PEER, bridge.STREAM_PORT = "127.0.0.1", "127.0.0.1", 0
    bridge.init_stream_server()
    address = bridge.stream_listener.getsockname()
    peers = [socket.create_connection(address) for _ in range(2)]
    try:
        for peer in peers:
            peer.settimeout(1)
        # No media state exists: standby still reaches every attached app.
        bridge.send_stream("MERO:POWER:standby=1")
        assert all(peer.recv(4096) == b"MERO:POWER:standby=1\n" for peer in peers)
        bridge.send_stream("MERO:POWER:standby=0")
        assert all(peer.recv(4096) == b"MERO:POWER:standby=0\n" for peer in peers)
        peers[0].close()
        replacement = socket.create_connection(address)
        peers.append(replacement)
        replacement.settimeout(1)
        bridge.send_stream("MERO:POWER:standby=0")
        assert replacement.recv(4096) == b"MERO:POWER:standby=0\n"
        peers[1].recv(4096)
        assert bridge.protocol_value("a|b\r\nc") == "a/b  c"
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            state = folder / "archmeros/cinema-mode.json"
            with patch.dict(os.environ, {"XDG_STATE_HOME": str(folder)}), patch.object(bridge.subprocess,"run") as monitors:
                monitors.return_value.stdout='[ {"name":"DP-2","dpmsStatus":true} ]'
                bridge._side_checked=0
                assert not bridge.cinema_active()
                state.parent.mkdir()
                state.write_text('{"keep":"DP-1"}')
                assert bridge.cinema_active()
                state.unlink()
                assert not bridge.cinema_active()
                monitors.return_value.stdout='[{"name":"DP-2","dpmsStatus":false}]'
                bridge._side_checked=0
                assert bridge.cinema_active()
                monitors.return_value.stdout='[{"name":"DP-2","dpmsStatus":true}]'
                bridge._side_checked=0
                assert not bridge.cinema_active()
            # A genuine ipaq driver alone is insufficient: reject other USB IDs.
            tty = folder / "sys/ttyUSB4"
            device = folder / "usb/serial/port"
            device.mkdir(parents=True)
            tty.mkdir(parents=True)
            (tty / "device").symlink_to(device)
            (device / "driver").symlink_to(folder / "drivers/ipaq")
            (folder / "usb/idVendor").write_text("045e\n")
            (folder / "usb/idProduct").write_text("ffff\n")
            assert sync.find_device(folder / "sys", folder / "dev") is None
            (folder / "usb/idProduct").write_text("00ce\n")
            assert sync.find_device(folder / "sys", folder / "dev") == folder / "dev/ttyUSB4"
            cc = dict(temp_C="17", FeelsLikeC="16", humidity="80", windspeedKmph="6",
                      winddir16Point="SE", pressure="1018", precipMM="0.0", uvIndex="2",
                      observation_time="02:00 PM", weatherDesc=[{"value": "Cloudy|rain\nnext"}])
            day = {"mintempC": "10", "maxtempC": "20", "hourly": [{"weatherDesc": [{"value": "Cloudy"}]}] * 8}
            data = {"current_condition": [cc], "nearest_area": [{"areaName": [{"value": "Farroupilha"}], "region": [{"value": "RS"}], "latitude": "-29.36", "longitude": "-50.81"}], "weather": [day] * 3}

            class Response:
                def __enter__(self): return self
                def __exit__(self, *args): pass
                def read(self): return json.dumps(data).encode()

            with patch.object(bridge.urllib.request, "urlopen", return_value=Response()):
                bridge.fetch_weather()
            assert "temp_c=17" in bridge.latest_weather_line
            assert "d3_max=20" in bridge.latest_weather_line
            assert "desc=Cloudy/rain next" in bridge.latest_weather_line
            assert "\n" not in bridge.latest_weather_line
            previous_fetch = bridge.last_weather_fetch
            bridge.last_weather_fetch = 0
            with patch.object(bridge.urllib.request, "urlopen", side_effect=OSError("offline")):
                bridge.fetch_weather()
            assert bridge.last_weather_fetch == 0  # Failure must not freshen cached data.
            bridge.last_weather_fetch = previous_fetch
            fake = folder / "bad.exe"
            fake.write_bytes(b"MZ" + bytes(254))
            try:
                deploy.verify_binary(fake)
                raise AssertionError("Corrupt PE accepted")
            except ValueError:
                pass
            card = folder / "card"
            (card / "MERO").mkdir(parents=True)
            (card / "MERO/mero-shell.exe").write_bytes(b"previous working shell")
            (card / "MERO/gallery.cfg").write_text("orient=3\n")
            (card / "factory.bin").write_bytes(b"factory data")
            with patch.object(Path, "is_mount", return_value=True), \
                 patch.object(deploy.subprocess, "check_output", return_value=deploy.UUID), \
                 patch.object(deploy.os, "sync"):
                sources = [p for p in (ROOT / "wince").glob("*/*.exe") if p.name != "mero-bt.exe"]
                with patch.object(Path, "glob", return_value=iter(sources)):
                    try:
                        deploy.deploy(card)
                        raise AssertionError("Deployment accepted a missing Bluetooth client")
                    except SystemExit:
                        pass
                assert not (card / "MERO/deploy-staging").exists()
                keyed = folder / "mero-agent.exe"
                keyed.write_bytes((ROOT / "wince/shell/mero-shell.exe").read_bytes())
                public_sources = list((ROOT / "wince").glob("*/*.exe")) + [keyed]
                with patch.object(Path, "glob", return_value=iter(public_sources)):
                    deploy.deploy(card)
            backups = list((card / "MERO/deploy-backups").glob("*/mero-shell.exe"))
            assert not (card / "MERO/mero-agent.exe").exists()
            assert len(backups) == 1 and backups[0].read_bytes() == b"previous working shell"
            assert (card / "MERO/mero-shell.exe").read_bytes() == (ROOT / "wince/shell/mero-shell.exe").read_bytes()
            assert (card / "MERO/gallery.cfg").read_text() == "orient=3\n"
            assert (card / "factory.bin").read_bytes() == b"factory data"
            # Compile the real shared CE receiver against a minimal POSIX shim.
            (folder / "winsock2.h").write_text("/* socket declarations supplied by test shim */\n")
            executable = folder / "check-live"
            subprocess.run(["cc", "-std=c99", "-I", str(folder), str(ROOT / "scripts/check_live.c"),
                            "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        for binary in (ROOT / "wince").glob("*/*.exe"):
            deploy.verify_binary(binary)
    finally:
        for peer in peers:
            peer.close()
        for peer in bridge.stream_clients:
            peer.close()
        bridge.stream_listener.close()
    print("PASS: TCP reconnect, cinema, weather/cache, USB identity, ARM PE validation, deployment backups")


if __name__ == "__main__":
    check()
