#!/usr/bin/env python3
"""Exercise real photo rendering, signed controls and native verified RAM intake."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
from unittest.mock import patch
from PIL import Image
from companion_ambient import Ambient, render, frame_image, FRAME_BYTES
from companion_live import BluetoothLive, signed, DEVICE

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    base = Path(directory)
    red, blue, empty = [base/n for n in ("red", "blue", "empty")]
    for folder in (red, blue, empty): folder.mkdir()
    Image.new("RGB", (480, 272), "red").save(red/"red.png")
    Image.new("RGB", (100, 400), "blue").save(blue/"portrait.png")
    (red/"source.txt").write_text("preserve")
    assert render(red/"red.png") == b"\0\xf8"*(240*136)
    assert len(render(blue/"portrait.png")) == FRAME_BYTES == 65280
    portrait=Image.new("RGB",(160,400),"green")
    portrait.paste("red",(0,0,160,45));portrait.paste("blue",(0,355,160,400));portrait.save(base/"safe.png")
    # Full-screen center crop: edge bands outside the viewport are cropped,
    # not shrunk into a small foreground over a blurred background.
    assert render(base/"safe.png")==b"\0\x04"*(240*136)
    for portrait_mode in (False,True):
        image=frame_image(base/"safe.png",portrait_mode)
        assert image.size==(240,136)
        assert set(image.getdata())=={(0,128,0)}
    wide=Image.new("RGB",(400,160),"green")
    wide.paste("red",(0,0,45,160));wide.paste("blue",(355,0,400,160));wide.save(base/"wide.png")
    assert set(frame_image(base/"wide.png",True).getdata())=={(0,128,0)}
    share = base/"share";share.mkdir()
    config = base/"ambient.json"
    config.write_text(json.dumps(dict(folders=[dict(name=p.name,path=str(p)) for p in (red,blue,empty)],interval=20)))
    ambient = Ambient(share, config)
    event = dict(track=ambient.token,action="ambient-ping",value="0")
    ambient.update();assert ambient.future is None # no device demand, no rendering
    assert ambient.action(dict(event,track="wrong")) == "folder_changed_press_again"
    assert ambient.action(event) == "ok"
    ambient.update();ambient.future.result(timeout=3)
    line = ambient.update();first = ambient.sha
    assert first == hashlib.sha256(b"\0\xf8"*(240*136)).hexdigest() and first in line
    assert len(list(share.glob("ambient-*.565"))) == 1
    ambient.next_frame=0;ambient.update();assert ambient.future is None # wait for actual display acknowledgement
    assert ambient.action(dict(event,action="ambient-shown",value=str(ambient.generation)))=="ok"
    assert not ambient.awaiting_display and ambient.next_frame>time.monotonic()
    assert ambient.action(dict(event,action="ambient-folder",value="1")) == "ok"
    assert not ambient.sha and json.loads(config.read_text())["selected"] == 1
    ambient.update();ambient.future.result(timeout=3)
    ambient.update();assert ambient.sha and ambient.sha != first
    # A completed old-folder worker must never publish after a selection change.
    ambient.action(dict(event,action="ambient-next"));ambient.update();ambient.future.result(timeout=3)
    ambient.action(dict(event,action="ambient-folder",value="2"));ambient.update()
    assert not ambient.sha and "No images" in ambient.status
    assert (red/"source.txt").read_text() == "preserve"
    assert ambient.action(dict(event,action="ambient-folder",value="99")) == "control_unavailable"
    # Exercise the existing authenticated event path, expiry and receipt dedup.
    state = base/"state";state.mkdir();key=b"a"*32;(state/"key").write_text(key.hex())
    live=BluetoothLive(share=share,state=state);live.load_key();live.ambient=ambient
    fields=dict(v=1,device=DEVICE,id="123-456",created=int(time.time()),action="ambient-pause",track=ambient.token,value="0")
    (share/"live-event.txt").write_bytes(signed(fields,key));live.event()
    assert live.result=="ok" and ambient.paused
    live.event();assert ambient.paused
    fields.update(id="123-457",created=int(time.time())-60)
    (share/"live-event.txt").write_bytes(signed(fields,key));live.event()
    assert live.result=="expired_press_again" and ambient.paused
    ambient.close()
    for i in range(5):(share/f"ambient-{i:064x}.565").write_bytes(b"owned")
    (share/"ambient-user.565").write_bytes(b"preserve")
    restarted=Ambient(share,config)
    assert len(restarted.retained)==2 and (share/"ambient-user.565").read_bytes()==b"preserve"
    restarted.close()

source=(root/"wince/bluetooth/mero-agent-runtime.h").read_text()
block=source[source.index("static void AgentAmbient("):source.index("static void AgentArtwork(")]
shim=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mero-auth.h"
#include <stdlib.h>
#include "mero-photo-codec.h"
typedef unsigned char BYTE;typedef unsigned int DWORD;
#define MERO_AMBIENT_BYTES 65280
#define INVALID_HANDLE_VALUE -1
typedef struct {DWORD demandTick;char sha[65];unsigned short pixels[32640];} MeroAmbient;
static MeroAmbient cache,*mero_ambient=&cache;
static char xml[65536];static int open=1,locked,calls,closed,mero_bus_mutex;
static unsigned int bytes=MERO_AMBIENT_BYTES;
static DWORD photoGetMs,photoDecodeMs,photoBytes;
static void *FindWindowW(const wchar_t *a,void *b){(void)a;(void)b;return open?&cache:NULL;}
static int MeroBusLock(void){assert(!locked);locked=1;return 1;}
static void ReleaseMutex(int unused){(void)unused;assert(locked);locked=0;}
static int MeroAmbientOpen(void){mero_ambient=&cache;return 1;}
static void MeroAmbientClose(void){mero_ambient=NULL;}
static unsigned int GetTickCount(void){return 10000;}
static int GetBody(const char *file,void *type,int handle,unsigned int *n){(void)type;(void)handle;assert(!locked);assert(!strncmp(file,"ambient-",8));calls++;*n=bytes;return 1;}
static void ClosePeer(void){closed++;}
BLOCK
int main(void){AuthHash h;BYTE digest[32];char sha[65];memset(xml,21,MERO_AMBIENT_BYTES);
AuthInit(&h);AuthUpdate(&h,xml,MERO_AMBIENT_BYTES);AuthFinal(&h,digest);AuthHex(digest,32,sha);
cache.demandTick=10000;AgentAmbient(sha);assert(calls==1&&!strcmp(cache.sha,sha)&&!locked);
AgentAmbient(sha);assert(calls==1);cache.sha[0]=0;xml[0]++;AgentAmbient(sha);assert(!cache.sha[0]&&calls==2);
xml[0]--;bytes--;AgentAmbient(sha);assert(!cache.sha[0]&&calls==3);
bytes++;cache.demandTick=0;AgentAmbient(sha);assert(calls==3);
cache.demandTick=10000;AgentAmbient("../../bad");assert(calls==3);
open=0;AgentAmbient(sha);assert(!mero_ambient&&!cache.sha[0]&&!cache.demandTick&&!locked);return 0;}
'''.replace("BLOCK",block)
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/"test.c").write_text(shim)
    subprocess.run(["cc","-std=c99","-I",str(root/"wince"),str(p/"test.c"),"-o",str(p/"test")],check=True)
    subprocess.run([str(p/"test")],check=True)
ui=(root/"wince/ambient/mero-ambient.c").read_text()
tap=ui[ui.index("static int WeatherTap("):ui.index("static LRESULT CALLBACK WindowProc(")]
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory)
    (p/"tap.c").write_text("#include <assert.h>\nstatic int weatherVisible=1,folderMenu=0;\n"+tap+"""
int main(void){assert(WeatherTap(20,60)&&!weatherVisible);
assert(WeatherTap(20,60)&&weatherVisible);assert(!WeatherTap(230,200)&&weatherVisible);
folderMenu=1;assert(!WeatherTap(20,60)&&weatherVisible);
folderMenu=0;assert(!WeatherTap(205,174)&&weatherVisible);return 0;}
""")
    subprocess.run(["cc",str(p/"tap.c"),"-o",str(p/"tap")],check=True)
    subprocess.run([str(p/"tap")],check=True)
queue=ui[ui.index("static int Queue("):ui.index("static void WeatherIcon(")]
shim=r'''
#include <assert.h>
#include <string.h>
#include <wchar.h>
typedef unsigned int DWORD;
static char queuedAction[32],queuedToken[32],token[32]="session-one",sent[32];
static unsigned int queuedValue;static DWORD queuedTick,lastPing,tick=100;
static int expectedFolder,fresh=1,busy=1,calls;static wchar_t notice[96];
#define lstrcpyW wcscpy
static DWORD GetTickCount(void){return tick;}
static int Fresh(void){return fresh;}
static int MeroBusCommand(const char *action,const char *track,unsigned int value){
assert(!strcmp(action,"ambient-folder"));assert(value==2);if(busy)return 0;strcpy(sent,track);calls++;return 1;}
BLOCK
int main(void){assert(Queue("ambient-folder",2));FlushQueue();assert(queuedAction[0]&&!calls);
assert(!Queue("ambient-next",0));strcpy(token,"session-two");busy=0;FlushQueue();
assert(!queuedAction[0]&&calls==1&&!strcmp(sent,"session-one")&&lastPing==100);
assert(Queue("ambient-folder",2));expectedFolder=2;tick+=15001;FlushQueue();
assert(!queuedAction[0]&&expectedFolder==-1&&calls==1);
fresh=0;assert(!Queue("ambient-folder",2));return 0;}
'''.replace("BLOCK",queue)
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory);(p/"queue.c").write_text(shim)
    subprocess.run(["cc","-std=c99",str(p/"queue.c"),"-o",str(p/"queue")],check=True)
    subprocess.run([str(p/"queue")],check=True)
# Actual bundled JPEG decoder: valid frame, bad dimensions and truncated header.
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory)
    Image.new("RGB",(240,136),"red").save(p/"ok.jpg")
    Image.new("RGB",(16,16),"red").save(p/"bad.jpg")
    (p/"codec.c").write_text(r'''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "mero-photo-codec.h"
int main(int argc,char **argv){unsigned char data[65536];unsigned short out[32640];int n;FILE *f=fopen(argv[1],"rb");n=fread(data,1,sizeof(data),f);fclose(f);
assert(MeroPhotoDecode(data,n,out));assert(out[0]==0xf800);assert(!MeroPhotoDecode(data,20,out));
f=fopen(argv[2],"rb");n=fread(data,1,sizeof(data),f);fclose(f);assert(!MeroPhotoDecode(data,n,out));return 0;}''')
    subprocess.run(["cc","-I",str(root/"wince"),str(p/"codec.c"),"-o",str(p/"codec")],check=True)
    subprocess.run([str(p/"codec"),str(p/"ok.jpg"),str(p/"bad.jpg")],check=True)
print("PASS: ambient rendering, folder races, source preservation, signed/expired/deduplicated controls, native frame hash/size/demand/close guards")
