#!/usr/bin/env python3
"""Exercise the agent's actual queued manual-action block."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'wince/bluetooth/mero-agent-runtime.h').read_text()
a=source.index('{LONG action=InterlockedExchange(&agentManualAction,-1);')
b=source.index('        if(!GetBody("agent-job.txt"',a)
block=source[a:b]
shim='''
#include <assert.h>
typedef long LONG;
static LONG agentManualAction=-1,agentManualBusy;
static unsigned long lastRequest;static int listed,downloaded,uploaded,window;
#define WM_BT_RESULT 1
static LONG InterlockedExchange(volatile LONG *p,LONG n){LONG old=*p;*p=n;return old;}
static unsigned long GetTickCount(void){return 50;}
static void ListFiles(void){assert(agentManualBusy);listed++;}
static void Download(void){assert(agentManualBusy);downloaded++;}
static void UploadTest(void){assert(agentManualBusy);uploaded++;}
static void PostMessageW(int h,int m,int w,int l){(void)h;(void)m;(void)w;(void)l;}
static void run(void){BLOCK}
int main(void){run();assert(!listed&&!downloaded&&!uploaded);
agentManualAction=0;run();assert(listed==1&&agentManualAction==-1&&!agentManualBusy&&lastRequest==50);
agentManualAction=2;run();assert(downloaded==1);
agentManualAction=3;run();assert(uploaded==1);run();assert(uploaded==1);return 0;}
'''.replace('BLOCK',block)
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'check.c').write_text(shim)
 subprocess.run(['cc',str(p/'check.c'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True)
print('PASS: queued manual list/download/upload, worker exclusion and no repeat')
