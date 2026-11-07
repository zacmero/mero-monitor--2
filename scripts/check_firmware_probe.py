#!/usr/bin/env python3
"""Check the probe's actual report writer bounds and read-only API surface."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "wince/probe/mero-firmware-probe.c").read_text()
for forbidden in ("RegSetValue", "RegCreateKey", "RegDelete", "DeviceIoControl", "KernelIoControl",
                  "SetDevicePower", "SetSystemPowerState", "PowerOffSystem", "FormatStore", "Dismount"):
    assert forbidden not in source, forbidden
writer = source[source.index("static void Line("):source.index("static void Utf8(")]
shim = r'''
#include <assert.h>
#include <string.h>
typedef unsigned int DWORD;
#define REPORT_LIMIT (256*1024)
static int output,healthy=1,truncated,fail,writes;
static DWORD total;
static int WriteFile(int h,const char *s,DWORD n,DWORD *done,void *o){(void)h;(void)s;(void)o;writes++;*done=fail?0:n;return !fail;}
WRITER
int main(void){Line("abc");assert(total==3&&writes==1&&healthy);
total=REPORT_LIMIT-2;Line("abc");assert(truncated&&total==REPORT_LIMIT-2&&writes==1);
Line("a");assert(total==REPORT_LIMIT-1&&writes==2);
total=0;fail=1;Line("x");assert(!healthy);return 0;}
'''.replace("WRITER", writer)
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    (p / "check.c").write_text(shim)
    subprocess.run(["cc", str(p / "check.c"), "-o", str(p / "check")], check=True)
    subprocess.run([str(p / "check")], check=True)
print("PASS: firmware probe report bounds/write errors; no prohibited setter or hardware-control APIs")
