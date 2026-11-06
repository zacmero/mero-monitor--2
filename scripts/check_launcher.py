#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
import struct
from foston_screen import decode

root=Path(__file__).resolve().parents[1]
source=(root/"wince/shell/mero-shell.c").read_text()
prefs=source[source.index("static void LoadConfig(void)"):source.index("/* Query system metrics */")]
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory);(folder/"shell_prefs.inc").write_text(prefs)
    subprocess.run(["cc","-std=c99","-I",str(folder),str(root/"scripts/check_launcher.c"),"-o",str(folder/"check")],check=True)
    subprocess.run([str(folder/"check")],check=True)
data=b"MERO32R1"+struct.pack("<HH",480,272)+struct.pack("<HH",480,0xf800)*272
w,h,rgb=decode(data);assert (w,h)==(480,272) and rgb==b"\xff\0\0"*(w*h)
for bad in (data[:-1],data+b"x",data[:12]+struct.pack("<HH",481,0),data[:12]+struct.pack("<HH",0,0)):
    try:decode(bad);raise AssertionError("accepted corrupt screenshot")
    except ValueError:pass
print("PASS: screenshot bounds and RGB565 decoding")
