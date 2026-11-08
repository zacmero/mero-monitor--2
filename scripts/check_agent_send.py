#!/usr/bin/env python3
"""Compile the real agent sender; verify packet bounds and exact file bodies."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "wince/bluetooth/mero-agent-runtime.h").read_text()
function = source[source.index("static int AgentSendData("):source.index("static int AgentOwnFile(")]
shim = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>
#include "mero-obex.h"
typedef unsigned int DWORD;typedef wchar_t WCHAR;typedef int HANDLE;
#define GENERIC_READ 0
#define FILE_SHARE_READ 0
#define OPEN_EXISTING 0
#define FILE_ATTRIBUTE_NORMAL 0
#define INVALID_HANDLE_VALUE -1
#define AGENT_MAX_FILE 16777216
static unsigned char packet[2048],reply[2048],input[4096],output[4096];
static unsigned int fileSize,cursor,outSize,maxPacket,calls,sendPackets,sendReply;
static int haveId=1,connectionId=7,fail;
static unsigned int peerLimit=1024;
static HANDLE CreateFileW(const WCHAR*p,int a,int b,void*c,int d,int e,void*f){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;cursor=0;return 1;}
static DWORD GetFileSize(HANDLE h,void*p){(void)h;(void)p;return fileSize;}
static int ReadFile(HANDLE h,void*p,DWORD n,DWORD*got,void*q){(void)h;(void)q;assert(cursor+n<=fileSize);memcpy(p,input+cursor,n);cursor+=n;*got=n;return 1;}
static void CloseHandle(HANDLE h){(void)h;}
static unsigned int Exchange(unsigned int n){
 unsigned int at=3,take;assert(n<=peerLimit&&n<=512&&Obex16(packet+1)==n);
 calls++;if(n>maxPacket)maxPacket=n;
 while(at<n){unsigned int tag=packet[at];
  if(tag==0x48||tag==0x49){take=Obex16(packet+at+1)-3;assert(outSize+take<=sizeof(output));memcpy(output+outSize,packet+at+3,take);outSize+=take;at+=take+3;}
  else if((tag&0xc0)==0xc0)at+=5;
  else at+=Obex16(packet+at+1);
 }
 reply[0]=packet[0]==0x82?0xa0:0x90;return fail?0:3;
}
'''
tests = r'''
int main(void){unsigned int i,j;const char*names[]={"live-event.txt","keyboard-result.txt","live-codec.txt","agent-output.bin"};
 for(i=0;i<sizeof(input);i++)input[i]=(unsigned char)i;
 for(j=0;j<4;j++)for(fileSize=0;fileSize<=4096;fileSize+=128){cursor=outSize=maxPacket=calls=0;
  assert(AgentSend(names[j],L"test"));assert(cursor==fileSize&&outSize==fileSize&&!memcmp(input,output,fileSize));
  assert(maxPacket<=(j<3?256:512)&&calls>0);
  cursor=outSize=maxPacket=calls=0;
  assert(AgentSendMemory(names[j],(const char*)input,fileSize));
  assert(outSize==fileSize&&!memcmp(input,output,fileSize)&&maxPacket<=(j<3?256:512));
 }
 fileSize=400;outSize=0;fail=1;assert(!AgentSend("live-event.txt",L"test"));fail=0;
 peerLimit=32;assert(!AgentSend("live-event.txt",L"test"));
 puts("PASS: actual agent control/bulk packet caps, exact bodies, empty files and errors");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "check.c").write_text(shim + function + tests)
    subprocess.run(["cc", "-std=c99", "-I", str(root / "wince"), str(path / "check.c"), "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)

# Actual event worker must not reset Bluetooth for a pre-transfer local error.
event=source[source.index("static void AgentLiveEvent("):source.index("static int AgentCompactActive(")]
shim=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef unsigned int DWORD;
#define MERO_AGENT_DEVICE "00:18:E4:1F:03:AF"
static struct {char event[512],eventId[80],ack[80];} bus,*mero_bus=&bus;
static int mero_bus_mutex,writeOk=1,sendOk=1,network=1,closes,calls,eventFirstOk,eventFirstWriteOk;
static unsigned int sendPackets,sendReply,eventFirstPackets,eventFirstReply;
static DWORD tick,eventQueueMs,eventWriteMs,eventFirstSendMs,eventFirstError,liveSendMs;
static char eventMeasuredId[80];static unsigned char agentKey[32];
static int MeroBusLock(void){return 1;}static void ReleaseMutex(int m){(void)m;}
static DWORD GetTickCount(void){return tick;}static DWORD GetLastError(void){return 5;}
static int AgentSign(char*a,unsigned int b,unsigned char*c){(void)a;(void)b;(void)c;return 1;}
static void CreateDirectoryW(const wchar_t*a,void*b){(void)a;(void)b;}
static int AgentWrite(const wchar_t*a,const void*b,unsigned int c){(void)a;(void)b;(void)c;return writeOk;}
static int AgentSendMemory(const char*a,const char*b,unsigned int n){(void)a;(void)b;(void)n;calls++;sendPackets=network;sendReply=network?160:0;return sendOk;}
static void ClosePeer(void){closes++;}
'''
tests=r'''
int main(void){strcpy(bus.event,"action=terminal-input\n");strcpy(bus.eventId,"1-2-100");tick=200;
 writeOk=0;sendOk=0;network=0;AgentLiveEvent();assert(!closes&&calls==1&&eventFirstError==5);
 network=1;tick=400;AgentLiveEvent();assert(closes==1&&calls==2);
 sendOk=1;tick=500;AgentLiveEvent();assert(closes==1&&calls==3);
 tick=600;AgentLiveEvent();assert(calls==3); /* Wait for signed ACK; no tight resend. */
 strcpy(bus.ack,bus.eventId);tick=2000;AgentLiveEvent();assert(calls==3);
 puts("PASS: actual RAM event sender preserves pending input, reconnects only on network failure and suppresses ACKed/resend duplicates");return 0;}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory);(path/"event.c").write_text(shim+event+tests)
    subprocess.run(["cc","-std=c99",str(path/"event.c"),"-o",str(path/"event")],check=True)
    subprocess.run([str(path/"event")],check=True)
