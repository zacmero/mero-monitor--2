#!/usr/bin/env python3
"""Offline preservation verification must detect loss, edits and extra files."""
from pathlib import Path
from tempfile import TemporaryDirectory
from foston_preserve import seal,verify
import json
with TemporaryDirectory() as directory:
 root=Path(directory);(root/"device").mkdir();item=root/"device/app.exe";item.write_bytes(b"known-good")
 seal(root);assert verify(root)==1
 for data in (b"changed",b"known-goof"):
  item.write_bytes(data)
  try:verify(root)
  except ValueError:pass
  else:raise AssertionError("Corruption accepted")
 item.write_bytes(b"known-good");extra=root/"unexpected";extra.touch()
 try:verify(root)
 except ValueError:pass
 else:raise AssertionError("Unexpected file accepted")
 extra.unlink();item.unlink()
 try:verify(root)
 except ValueError:pass
 else:raise AssertionError("Missing file accepted")
 item.symlink_to('/etc/hosts')
 try:verify(root)
 except ValueError:pass
 else:raise AssertionError("Symlink accepted")
print("PASS: private snapshot hash verification rejects corruption, missing/extra files and symlinks")

# Exercise the collector against a fake signed-read client: no mutation operation
# or unrelated host secret may enter this path.
from unittest.mock import patch
import hashlib
import foston_preserve as preserve
with TemporaryDirectory() as directory:
 home=Path(directory)/"home";state=home/".config/mero/foston-agent";state.mkdir(parents=True)
 (state/"sequence").write_text("42");(state/"key").write_text("test-only")
 (home/".config/mero/ambient.json").write_text("{}")
 (home/".config/mero/unrelated-secret").write_text("excluded")
 (home/".config/systemd/user").mkdir(parents=True)
 calls=[]
 class FakeClient:
  def __init__(self,*args,**kwargs):pass
  def locked(self):
   from contextlib import nullcontext
   return nullcontext()
  def rpc(self,op,path="",output=None):
   calls.append(op);assert op in ("status","ls","get")
   result=dict(seq=str(len(calls)),ok="1")
   if op=="status":result.update(agent_path="\\SDMMC\\BT\\Downloads\\mero-agent.exe",agent_sha256=hashlib.sha256(b"agent").hexdigest())
   elif op=="ls":result["listing"]="file\t5\tapp.exe\n" if path.casefold()=="\\sdmmc\\mero" else ""
   else:
    output.write_bytes(b"agent" if path.endswith("mero-agent.exe") else b"app")
   return result
 with patch.object(preserve,"Client",FakeClient),patch.object(Path,"home",return_value=home),patch.object(preserve.subprocess,"run",return_value=type("Result",(),{"stdout":"ActiveState=active\n"})()):
  root=Path(directory)/"snapshot";snapshot_count=preserve.snapshot(root)
 assert snapshot_count==preserve.verify(root)
 assert not (root/"host/.config/mero/unrelated-secret").exists()
 assert (root/"host/.config/mero/ambient.json").exists()
 assert calls.count("status")==2 and calls.count("get")==2
print("PASS: collector uses only status/ls/get, verifies running identity and excludes unrelated secrets")
