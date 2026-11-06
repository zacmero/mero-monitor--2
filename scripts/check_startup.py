#!/usr/bin/env python3
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/"wince/bluetooth/mero-agent-runtime.h").read_text()
a=source.index("static void AgentLoop(void)");b=source.index('    Note(L"Agent ON:',a)
prefix=source[a:b]+"ready=1;\nfinish: if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}\n}\n"
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory);(folder/"agent-init-prefix.inc").write_text(prefix)
    subprocess.run(["cc","-std=c99","-I",str(folder),str(root/"scripts/check_startup.c"),"-o",str(folder/"check")],check=True)
    subprocess.run([str(folder/"check")],check=True)
