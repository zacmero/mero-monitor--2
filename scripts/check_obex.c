#include <assert.h>
#include <stdio.h>
#include "../wince/mero-obex.h"
int main(void) {
    unsigned char p[4096];unsigned int n,pos,payload,length;
    n=ObexConnect(p);assert(n==26&&Obex16(p+1)==26&&Obex16(p+5)==4096);
    assert(p[7]==0x46&&Obex16(p+8)==19&&p[10]==0xf9&&p[25]==9);
    n=ObexRequest(p,0x83,1,0x12345678,"mero-shell.exe",NULL);
    assert(p[0]==0x83&&Obex16(p+1)==n&&p[3]==0xcb&&Obex32(p+4)==0x12345678);
    pos=ObexHeader(p,n,3,&payload,&length);assert(pos==8&&length==4);
    pos=ObexHeader(p,n,pos,&payload,&length);assert(pos==n&&length==30);
    assert(p[payload]==0&&p[payload+1]=='m'&&p[n-1]==0&&p[n-2]==0);
    n=ObexRequest(p,0x83,0,0,NULL,"x-obex/folder-listing");
    assert(p[3]==0x42&&ObexHeader(p,n,3,&payload,&length)==n);
    assert(!strcmp((char*)p+payload,"x-obex/folder-listing"));
    /* Real malformed-response boundaries, including integer and body headers. */
    p[0]=0x49;p[1]=0;p[2]=2;assert(!ObexHeader(p,3,0,&payload,&length));
    p[2]=4;assert(!ObexHeader(p,3,0,&payload,&length));
    p[2]=3;assert(ObexHeader(p,3,0,&payload,&length)==3&&length==0);
    p[0]=0xcb;assert(!ObexHeader(p,4,0,&payload,&length));
    assert(ObexHeader(p,5,0,&payload,&length)==5&&length==4);
    assert(!ObexHeader(p,5,5,&payload,&length));
    puts("PASS: OBEX FTP target, connection ID, Unicode names, folder listing, malformed headers");
    return 0;
}
