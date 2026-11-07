#!/usr/bin/env python3
"""Run the installed native minimum-brightness action over authenticated Bluetooth."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def rpc(*args):
    result = subprocess.run(["rtk", "proxy", sys.executable, str(ROOT / "scripts/foston_agent.py"), *args],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return json.loads(result.stdout)


def minimum():
    launched = rpc("launch", "mero-backlight-min.exe")
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "report.json"
        for _ in range(3):
            rpc("get", "/SDMMC/MERO/backlight-manual.txt", str(path))
            report = json.loads(path.read_text())
            if str(report["pid"]) == launched["value"]:
                if report["ok"] != 1 or report["after"] != 14:
                    raise RuntimeError(f"Device rejected minimum brightness: {report}")
                return report
    raise RuntimeError("No fresh brightness result; do not assume the action completed")


if __name__ == "__main__":
    print(json.dumps(minimum(), indent=2))
