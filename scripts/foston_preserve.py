#!/usr/bin/env python3
"""Read-only device-file preservation. Private output, not a firmware image.

snapshot DIR collects signed reads plus host agent state under the client lock.
verify DIR checks the exact manifest without running device commands.
Never restores credentials, counters, boot settings or files automatically.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from foston_agent import Client, device_path


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream,"sha256").hexdigest()


def seal(root):
    if any(p.is_symlink() for p in root.rglob("*")):raise ValueError("Symlinks are not allowed in a recovery bundle")
    files={str(p.relative_to(root)):dict(bytes=p.stat().st_size,sha256=digest(p))
           for p in root.rglob("*") if p.is_file() and p != root/"manifest.json"}
    (root/"manifest.json").write_text(json.dumps(dict(format=1,kind="file-backup-NOT-full-firmware",files=files),indent=2)+"\n")
    for p in root.rglob("*"):
        p.chmod(0o700 if p.is_dir() else 0o600)
    return files


def verify(root):
    if root.is_symlink() or any(p.is_symlink() for p in root.rglob("*")):raise ValueError("Symlinks are not allowed in a recovery bundle")
    manifest=json.loads((root/"manifest.json").read_text())
    if manifest.get("format")!=1:raise ValueError("Unsupported manifest")
    actual={str(p.relative_to(root)) for p in root.rglob("*") if p.is_file() and p!=root/"manifest.json"}
    if actual!=set(manifest["files"]):raise ValueError("Missing or unexpected files")
    for name,item in manifest["files"].items():
        p=root/name
        if p.is_symlink() or not p.resolve().is_relative_to(root.resolve()):raise ValueError("Unsafe manifest path")
        if p.stat().st_size!=item["bytes"] or digest(p)!=item["sha256"]:raise ValueError("Hash mismatch: "+name)
    return len(actual)


def snapshot(root):
    root.mkdir(mode=0o700,parents=True,exist_ok=False)
    os.umask(0o077)
    state=Path.home()/".config/mero/foston-agent"
    client=Client(state,Path.home()/".local/share/mero-foston-bluetooth",timeout=120)
    receipts=root/"receipts";receipts.mkdir()
    def rpc(op,path="",output=None):
        result=client.rpc(op,device_path(path) if path else "",output=output)
        (receipts/(result["seq"]+".json")).write_text(json.dumps(result,indent=2)+"\n")
        return result
    with client.locked():
        before=rpc("status")
        paths=set()
        for directory in ("/SDMMC/MERO","/ResidentFlash/MERO","/SDMMC/BT","/SDMMC/BT/Downloads","/SDMMC"):
            listing=rpc("ls",directory)["listing"]
            for line in listing.splitlines():
                kind,size,name=line.split("\t",2)
                if kind!="file":continue
                if name in (".","..") or "/" in name or "\\" in name:raise ValueError("Unsafe listing name")
                if directory=="/SDMMC" and name!="shell.ini":continue
                if int(size)>16*1024*1024:raise ValueError("File exceeds signed GET limit: "+name)
                paths.add(directory+"/"+name)
        paths.add(before["agent_path"].replace("\\","/"))
        for path in sorted(paths):
            output=root/"device"/path.lstrip("/")
            output.parent.mkdir(parents=True,exist_ok=True)
            result=rpc("get",path,output)
            if path.casefold()==before["agent_path"].replace("\\","/").casefold() and digest(output)!=before["agent_sha256"]:
                raise ValueError("Agent executable differs from running identity")
            print(json.dumps(dict(path=path,seq=result["seq"],bytes=output.stat().st_size)),flush=True)
        after=rpc("status")
        if before["agent_sha256"]!=after["agent_sha256"]:raise ValueError("Agent changed during snapshot")
        if (state/"pending.json").exists():raise ValueError("Unresolved job; do not snapshot host counters")
        shutil.copytree(state,root/"host/foston-agent")
        for path in (Path.home()/".config/mero",Path.home()/".config/systemd/user"):
            dest=root/"host"/path.relative_to(Path.home())
            dest.mkdir(parents=True,exist_ok=True)
            for src in path.iterdir():
                if src.is_file() and not src.is_symlink() and ((path.name=="mero" and (src.name=="ambient.json" or src.name=="weather.conf")) or (path.name=="user" and src.name.startswith(("mero-bridge.","mero-bt-ftp.")))):
                    shutil.copy2(src,dest/src.name)
        for service in ("mero-bt-ftp.service","mero-bridge.service"):
            result=subprocess.run(["systemctl","--user","show",service,"-p","ActiveState","-p","FragmentPath","-p","ExecStart"],capture_output=True,text=True,check=True)
            (root/"host"/(service+".txt")).write_text(result.stdout)
        (root/"README.txt").write_text("PRIVATE: contains authentication keys, user configuration and device binaries.\nFile snapshot, NOT a full firmware/bootloader/SD image.\nDo not restore old counters or journals onto a newer running device.\nDynamic logs are point-in-time reads, not a transactional filesystem snapshot.\nSee repository docs/RECOVERY.md before any restoration.\n")
    seal(root)
    return verify(root)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation",choices=("snapshot","verify"));parser.add_argument("directory",type=Path)
    args=parser.parse_args()
    try:
        count=snapshot(args.directory) if args.operation=="snapshot" else verify(args.directory)
    except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as error:
        print(str(error),file=sys.stderr);return 1
    print(json.dumps(dict(ok=True,verified_files=count,directory=str(args.directory))))
    return 0

if __name__=="__main__":sys.exit(main())
