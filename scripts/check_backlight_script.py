#!/usr/bin/env python3
"""The host wrapper must require a fresh native result and verified minimum."""
import json
from pathlib import Path
from unittest.mock import patch
import foston_backlight_min as command


def run_case(pids, ok=1, after=14):
    replies = iter(pids)
    def rpc(*args):
        if args[0] == "launch":
            return {"value": "123"}
        Path(args[2]).write_text(json.dumps(dict(pid=next(replies), ok=ok, after=after)))
    with patch.object(command, "rpc", side_effect=rpc):
        return command.minimum()


assert run_case([99, 123])["pid"] == 123
for pids, ok, after in [([99, 99, 99], 1, 14), ([123], 0, 14), ([123], 1, 19)]:
    try:
        run_case(pids, ok, after)
        raise AssertionError("accepted stale or failed minimum result")
    except RuntimeError:
        pass
print("PASS: minimum script rejects stale process replies and unsuccessful brightness results")
