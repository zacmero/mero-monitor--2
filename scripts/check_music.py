#!/usr/bin/env python3
"""Run the native deck's actual snapshot/progress functions on the host."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"wince/music/mero-music.c").read_text()
functions=source[source.index("static void HostLine("):source.index("static void Artwork(")]
shim=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
typedef unsigned int DWORD;typedef wchar_t WCHAR;
#define FALSE 0
#define lstrcpyW wcscpy
static int portrait=1;
static int mainWindow,canNext,canPrevious,canPlay,canSeek,haveState;
static unsigned int tick,received,position,duration;
static char track[32],playback[24],artHash[65],paintedArt[65];static WCHAR title[192],artist[192],notice[96];
#define MERO_ART_SIDE 272
static DWORD displayArt[272*272],*artBits=displayArt;
static int mero_bus_mutex;
static struct {char sha[65],track[32];unsigned short pixels[272*272];} cachedArt,*mero_art=&cachedArt;
static int MeroArtOpen(void){return 1;}
static int MeroBusLock(void){return 1;}
static void ReleaseMutex(int mutex){(void)mutex;}
static DWORD GetTickCount(void){return tick;}
static void InvalidateRect(int w,void *r,int e){(void)w;(void)r;(void)e;}
static void Wide(WCHAR *out,int cap,const char *s){int i;for(i=0;s[i]&&i<cap-1;i++)out[i]=(unsigned char)s[i];out[i]=0;}
'''
tests=r'''
static void frame(const char *text){char line[1024];strcpy(line,text);HostLine(line);}
int main(void){
 int x=479-441,y=137;
 MusicCoordinates(&x,&y);assert(x==137&&y==441); /* Portrait play button. */
 portrait=0;x=240;y=210;MusicCoordinates(&x,&y);assert(x==240&&y==210);
 portrait=1;
 frame("MERO:AMB:folder=test|portrait=0|weather=1");assert(!portrait&&!haveState);
 frame("MERO:AMB:portrait=1|folder=test");assert(portrait&&!haveState);
 tick=100000;frame("MERO:NOW:status=PLAYING|title=First|artist=Artist|track=abcdefghijk|pos=10|len=100|playpause=1|next=1|seek=1|art=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
 assert(Fresh()&&canPlay&&canNext&&canSeek&&duration==100&&CurrentPosition()==10);
 assert(strlen(artHash)==64);
 tick+=5000;assert(CurrentPosition()==15);
 frame("MERO:NOW:status=PAUSED|title=First|artist=Artist|track=abcdefghijk|pos=15|len=100|playpause=1");
 tick+=3000;assert(CurrentPosition()==15&&!canSeek&&!canNext);
 memset(displayArt,255,sizeof(displayArt));memset(&cachedArt,255,sizeof(cachedArt));
 strcpy(cachedArt.sha,"old");strcpy(cachedArt.track,"abcdefghijk");strcpy(paintedArt,"old");
 frame("MERO:NOW:status=PLAYING|title=Second|artist=Other|track=lmnopqrstuv|pos=0|len=200|playpause=1|next=1");
 assert(!strcmp(track,"lmnopqrstuv")&&position==0&&duration==200&&CurrentPosition()==0);
 assert(!artHash[0]); /* Clear previous artwork while the new cover loads. */
 assert(!paintedArt[0]&&!cachedArt.sha[0]&&!cachedArt.pixels[0]&&!displayArt[0]);
 tick+=16000;assert(!Fresh()&&CurrentPosition()==15);
 tick+=100000;assert(CurrentPosition()==15); /* Freeze stale progress. */
 frame("MERO:NOW:status=PLAYING|title=Unknown timing|track=lmnopqrstuv|pos=0|len=0");
 assert(duration==0&&!canPlay&&!canNext&&!canSeek);
 frame("MERO:NOW:status=UNAVAILABLE|title=Open music|track=|pos=0|len=0");assert(!Fresh());
 puts("PASS: native music orientation/touch transform, track changes, progress, pause and stale freeze");return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory);(path/"check.c").write_text(shim+functions+tests)
    subprocess.run(["cc","-std=c99",str(path/"check.c"),"-o",str(path/"check")],check=True)
    subprocess.run([str(path/"check")],check=True)
