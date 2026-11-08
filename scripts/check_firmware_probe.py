#!/usr/bin/env python3
"""Check the probe's actual report writer bounds and read-only API surface."""
from pathlib import Path
import subprocess
import re
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "wince/probe/mero-firmware-probe.c").read_text()
# Symbol names in the export inventory are data. Reject actual calls after
# removing comments/string literals; inventory pointers are never invoked.
code=re.sub(r'/\*.*?\*/|//[^\n]*|L?"(?:\\.|[^"\\])*"', ' ', source, flags=re.S)
for forbidden in ("RegSetValue", "RegCreateKey", "RegDelete", "DeviceIoControl", "KernelIoControl",
                  "SetDevicePower", "SetSystemPowerState", "PowerOffSystem", "FormatStore", "Dismount",
                  "VirtualCopy", "SetKMode", "SetProcPermissions", "WriteProcessMemory", "RegSaveKey", "RegCopyFile", "LoadLibrary"):
    assert not re.search(r'\b'+forbidden+r'\w*\s*\(',code), forbidden
assert 'SPI_SET' not in code
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

# Compile the actual export inventory with non-callable sentinel addresses.
# Executing any advertised setter/kernel function would fail this check.
inventory=source[source.index("static void ApiInventory("):source.index("static void ModuleInventory(")]
shim=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
typedef wchar_t WCHAR;typedef uintptr_t DWORD;typedef void *HMODULE;typedef void *FARPROC;
static int looked,lines;
static HMODULE GetModuleHandleW(const WCHAR*n){return wcscmp(n,L"coredll.dll")?NULL:(void*)1;}
static FARPROC GetProcAddress(HMODULE h,const WCHAR*n){assert(h==(void*)1&&n[0]);looked++;return (void*)1;}
static void Utf8(const WCHAR*w,char*out,int cap){int i;for(i=0;w[i]&&i<cap-1;i++)out[i]=(char)w[i];out[i]=0;}
static void Line(const char*s){assert(strlen(s)<700);lines++;}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory);(path/"inventory.c").write_text(shim+inventory+'\nint main(void){ApiInventory();assert(looked>10&&lines>looked);return 0;}')
    subprocess.run(["cc",str(path/"inventory.c"),"-o",str(path/"inventory")],check=True)
    subprocess.run([str(path/"inventory")],check=True)
print("PASS: actual export inventory only resolves metadata; no advertised function is invoked")
