#!/usr/bin/env python3
"""Actual CE decoder, authenticated codec lease and legacy fallback."""
import base64
import ctypes
from pathlib import Path
import subprocess
import tempfile
import time
import zlib
from types import SimpleNamespace
from companion_live import BluetoothLive,DEVICE,signed,verified

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory)
    wrapper=folder/'codec.c'
    wrapper.write_text('''#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mero-live-codec.h"
#include "mero-photo-codec.h"
#include "mero-live-zlib.h"
int decode(const char *data,const char *codec,char *out,unsigned int cap){return MeroLiveDecode(data,codec,out,cap);}
''')
    subprocess.run(['cc','-O2','-shared','-fPIC','-I',str(root/'wince'),str(wrapper),'-o',str(folder/'codec.so'),'-lm'],check=True)
    native=ctypes.CDLL(str(folder/'codec.so'));native.decode.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_void_p,ctypes.c_uint];native.decode.restype=ctypes.c_int
    def decode(data,codec=b'zlib1',cap=4096):
        output=ctypes.create_string_buffer(4096)
        n=native.decode(base64.b64encode(data),codec,output,cap)
        return n,output.raw[:max(n,0)]
    payload=(b'MERO:TERM:status=PC SESSION|text=abcd1234\n'*20)
    assert decode(zlib.compress(payload))==(len(payload),payload)
    assert decode(zlib.compress(payload,0))==(len(payload),payload)
    assert decode(payload,b'')==(len(payload),payload)
    assert decode(zlib.compress(payload),cap=len(payload))[0]<0
    for bad in [b'',b'bad',zlib.compress(payload)[:-2],zlib.compress(payload)[:-1]+b'x',zlib.compress(b'A'*5000),zlib.compress(b'a\0b')]:
        assert decode(bad)[0]<0
    assert decode(zlib.compress(payload),b'unknown')[0]<0
    assert decode(zlib.compress(payload),cap=0)[0]<0
    assert decode(zlib.compress(payload),cap=4097)[0]<0
    share=folder/'share';state=folder/'state';share.mkdir();state.mkdir();key=b'k'*32;(state/'key').write_text(key.hex())
    live=BluetoothLive(share,state)
    def publish():
        live.publish([payload.decode().rstrip()]);return verified((share/'live-state.txt').read_bytes(),key)
    assert publish()['codec']==''
    def capability(**changes):
        fields=dict(v=1,device=DEVICE,offer=live.codec_offer,codec='zlib1',epoch=int(time.time()));fields.update(changes)
        (share/'live-codec.txt').write_bytes(signed(fields,key))
    capability();packet=publish();assert packet['codec']=='zlib1'
    assert zlib.decompress(base64.b64decode(packet['data']))==payload
    capability(offer='old-session');assert publish()['codec']==''
    capability(epoch=int(time.time())-20);assert publish()['codec']==''
    capability();raw=(share/'live-codec.txt').read_bytes();(share/'live-codec.txt').write_bytes(raw.replace(b'zlib1',b'zlib2'));assert publish()['codec']==''
    live.terminal=SimpleNamespace(demand_until=0)
    capability(terminal='1');publish();assert live.terminal.demand_until>time.monotonic()
    capability(terminal='0');publish();assert live.terminal.demand_until==0
    (share/'agent-job.txt').write_bytes(signed(dict(v=1,device=DEVICE,seq=123,op='status'),key))
    assert publish()['job_seq']=='123'
    (share/'agent-job.txt').write_bytes(signed(dict(v=1,device=DEVICE,seq=0,op='idle'),key))
    assert publish()['job_seq']=='0'
source=(root/'wince/bluetooth/mero-agent-runtime.h').read_text();section=source[source.index('static void AgentLive(void)'):]
assert section.index('AgentVerifyLimit(')<section.index('MeroLiveDecode(')
assert section.index('AgentVerifyLimit(')<section.index('liveJobHintValid=')
gate=source[source.index('        if(AgentTerminalActive()&&liveJobHintValid'):source.index('        lastJobPoll=GetTickCount();')]
with tempfile.TemporaryDirectory() as directory:
    folder=Path(directory)
    c='''#include <assert.h>
#include <stdint.h>
typedef uint32_t DWORD;
static int fast,liveJobHintValid;static unsigned int liveJobHint;static DWORD liveJobHintTick,tick;
static int AgentTerminalActive(void){return fast;}
static DWORD GetTickCount(void){return tick;}
static int due(DWORD lastJobPoll,unsigned int sent){GATE return 1;pause:return 0;}
int main(void){tick=10000;fast=1;liveJobHintValid=1;liveJobHintTick=9000;
liveJobHint=0;assert(!due(1,5));liveJobHint=5;assert(!due(1,5));
liveJobHint=6;assert(due(1,5));assert(!due(9500,5));
liveJobHint=0;liveJobHintTick=6000;assert(due(1,5));
liveJobHintValid=0;assert(due(1,5));fast=0;assert(due(9999,5));
fast=1;liveJobHintValid=1;tick=5;liveJobHintTick=0xfffffff0;assert(!due(1,5));return 0;}
'''.replace('GATE',gate)
    (folder/'gate.c').write_text(c)
    subprocess.run(['cc',str(folder/'gate.c'),'-o',str(folder/'gate')],check=True)
    subprocess.run([str(folder/'gate')],check=True)
print('PASS: actual bounded zlib/base64 decode, checksum/truncation/zip-bomb rejection, signed nonce lease and legacy fallback')
