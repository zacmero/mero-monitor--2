#!/usr/bin/env python3
"""Exercise the native app's actual button actions with a bounded fake driver."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "wince/backlight/mero-backlight.c").read_text()
actions = source[source.index("static void SetLevel("):source.index("static void Refresh(")]
shim = r'''
#include <assert.h>
#include <wchar.h>
typedef unsigned int DWORD;
static DWORD currentRaw=14,originalRaw=39,driver=14;
static int ready=1,haveOriginal=1,fail,writes;
static wchar_t message[128];
#define lstrcpyW wcscpy
static int MeroBrightness(DWORD *raw,int write){if(fail)return 0;if(write){assert(*raw>=14&&*raw<=64&&(*raw-14)%5==0);driver=*raw;writes++;}else *raw=driver;return 1;}
ACTIONS
int main(void){Act(0);assert(driver==14);Act(1);assert(driver==19&&currentRaw==19);
Act(3);assert(driver==64);Act(1);assert(driver==64);Act(2);assert(driver==14);
Act(4);assert(driver==39&&currentRaw==39);int before=writes;
ready=0;Act(3);assert(writes==before);ready=1;fail=1;Act(3);assert(driver==39&&currentRaw==39);
return 0;}
'''.replace("ACTIONS", actions)
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    (p / "test.c").write_text(shim)
    subprocess.run(["cc", str(p / "test.c"), "-o", str(p / "test")], check=True)
    subprocess.run([str(p / "test")], check=True)
print("PASS: actual backlight app buttons, min/max clamps, restore and driver-failure handling")
