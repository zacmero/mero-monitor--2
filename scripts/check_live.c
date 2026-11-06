/* Exercise the actual CE protocol/recovery code with a small POSIX platform shim. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>
#include <assert.h>

typedef unsigned int DWORD;
typedef int BOOL, SOCKET, HWND, WSADATA;
typedef unsigned long u_long;
typedef int HANDLE;
typedef int HDC,HBRUSH;
typedef unsigned int UINT,WPARAM;
typedef long LPARAM,LRESULT;
#define CALLBACK
#define WM_PAINT 15
#define WM_ERASEBKGND 20
#define BLACK_BRUSH 4
typedef struct {int left,top,right,bottom;} RECT;
typedef struct {int unused;} PAINTSTRUCT;
typedef struct {
    LRESULT (*lpfnWndProc)(HWND,UINT,WPARAM,LPARAM);
    int hInstance;HBRUSH hbrBackground;const wchar_t *lpszClassName;
} WNDCLASSW;
static int black_fills;
static int GetStockObject(int which){return which;}
static int RegisterClassW(WNDCLASSW *cls){assert(cls->hbrBackground==BLACK_BRUSH);return 1;}
static HDC BeginPaint(HWND h,PAINTSTRUCT *p){return 1;}
static void EndPaint(HWND h,PAINTSTRUCT *p){}
static void GetClientRect(HWND h,RECT *r){r->left=r->top=0;r->right=480;r->bottom=272;}
static void FillRect(HDC dc,RECT *r,HBRUSH b){assert(b==BLACK_BRUSH);black_fills++;}
static LRESULT DefWindowProcW(HWND h,UINT m,WPARAM w,LPARAM l){return 0;}
#define INVALID_HANDLE_VALUE (-1)
#define PAGE_READWRITE 4
#define FILE_MAP_ALL_ACCESS 7
#define WAIT_OBJECT_0 0
#define WAIT_ABANDONED 128
static char shared_memory[8192];
static int CreateMutexW(void *a,int b,const wchar_t *name){return 1;}
static int CreateFileMappingW(int f,void *a,int p,int h,int size,const wchar_t *name){assert(size<8192);return 1;}
static void *MapViewOfFile(int m,int a,int h,int l,int size){return shared_memory;}
static int WaitForSingleObject(int m,int timeout){return 0;}
static void ReleaseMutex(int m){}
static void CloseHandle(int h){}
static void UnmapViewOfFile(void *p){}
static unsigned long GetCurrentProcessId(void){return 123;}
#define TRUE 1
#define FALSE 0
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define WSAEWOULDBLOCK EAGAIN
#define WSAEINPROGRESS EINPROGRESS
#define closesocket close
#define ioctlsocket ioctl
#define WSAGetLastError() errno
#define WS_EX_TOPMOST 1
#define WS_POPUP 2
#define SS_BLACKRECT 4
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SW_SHOW 1
#define SW_HIDE 0
#define MAKEWORD(a,b) (a)
#define NULL_HWND 0
static DWORD tick;
static BOOL visible;
static char received[256];
static DWORD GetTickCount(void) { return tick; }
static int GetSystemMetrics(int metric) { return metric ? 272 : 480; }
static int GetModuleHandleW(void *name) { return 0; }
static HWND CreateWindowExW(int ex, const wchar_t *cls, const wchar_t *text,
    int style, int x, int y, int w, int h, HWND owner, void *menu, int instance, void *param) { return 1; }
static void ShowWindow(HWND window, int show) { visible = show; }
static void InvalidateRect(HWND window, void *rect, int erase) {}
static void DestroyWindow(HWND window) { visible = FALSE; }
static int WSAStartup(int version, WSADATA *data) { return 0; }
static void WSACleanup(void) {}
static void SetTimer(HWND owner, int id, int period, void *callback) {}
static void KillTimer(HWND owner, int id) {}

/* Keep NULL initializers compatible with the lightweight integer test handles. */
#undef NULL
#define NULL 0
#include "../wince/mero-live.h"
#include "../wince/mero-live-codec.h"

static void capture(char *line) { snprintf(received, sizeof(received), "%s", line); }

int main(void)
{
    int pair[2];
    tick = 20000;
    MeroLiveInit(1, capture);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    fcntl(pair[0], F_SETFL, O_NONBLOCK);
    mero_socket = pair[0];
    mero_session = tick;
    assert(write(pair[1], "MERO:POWER:standby=", 19) == 19);
    MeroLiveTick(1);
    assert(!mero_standby && !visible);
    assert(write(pair[1], "1\nMERO:WX:temp_c=17\n", 20) == 20);
    MeroLiveTick(1);
    assert(mero_standby && visible);
    MeroBlankProc(1,WM_PAINT,0,0);assert(black_fills==1);
    assert(strcmp(received, "MERO:WX:temp_c=17") == 0);
    assert(write(pair[1], "MERO:POWER:standby=0\n", 21) == 21);
    MeroLiveTick(1);
    assert(!mero_standby && !visible);
    /* A disconnected host must wake the screen even during reconnect early returns. */
    MeroLiveStandby(1, TRUE);
    MeroLiveDisconnect();
    close(pair[1]);
    tick += 16000;
    mero_attempt = tick;
    MeroLiveTick(1);
    assert(!mero_standby && !visible);
    /* DWORD wrap-around must preserve the timeout. */
    mero_received = 0xfffffff0u;
    tick = 16000;
    mero_attempt = tick;
    MeroLiveStandby(1, TRUE);
    MeroLiveTick(1);
    assert(!mero_standby);
    /* Reject an oversized unterminated packet instead of overrunning the buffer. */
    {
        char oversized[8191];
        memset(oversized, 'X', sizeof(oversized));
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
        fcntl(pair[0], F_SETFL, O_NONBLOCK);
        mero_socket = pair[0];
        mero_session = tick;
        assert(write(pair[1], oversized, sizeof(oversized)) == sizeof(oversized));
        MeroLiveTick(1);
        assert(mero_socket == INVALID_SOCKET && mero_rx_len == 0);
        close(pair[1]);
    }
    /* Bluetooth snapshots arrive even while TCP reconnect returns early. */
    tick += 100;
    mero_attempt = tick;
    strcpy(mero_bus->lines,"MERO:POWER:standby=1\nMERO:WX:temp_c=22\n");
    mero_bus->tick=tick;mero_bus->epoch=1700000000;mero_bus->generation=1;
    MeroLiveTick(1);
    assert(mero_standby && !strcmp(received,"MERO:WX:temp_c=22"));
    assert(MeroBusCommand("next","abcdefghijk",0));
    assert(!MeroBusCommand("next","abcdefghijk",0));
    strcpy(mero_bus->ack,mero_bus->eventId);
    tick++;
    assert(MeroBusCommand("playpause","abcdefghijk",0));
    tick+=16000;mero_attempt=tick;
    MeroLiveTick(1);assert(!mero_standby);
    assert(!MeroBusCommand("next","abcdefghijk",0));
    {char decoded[32];assert(MeroBase64("aMOpbGxv",decoded,sizeof(decoded))==6);
     assert(!strcmp(decoded,"h\xc3\xa9llo"));
     assert(MeroBase64("YQ==",decoded,sizeof(decoded))==1 && !strcmp(decoded,"a"));
     assert(MeroBase64("YQ=",decoded,sizeof(decoded))<0);
     assert(MeroBase64("YR==",decoded,sizeof(decoded))<0);
     assert(MeroBase64("YQ==x",decoded,sizeof(decoded))<0);
     assert(MeroBase64("AA==",decoded,sizeof(decoded))<0);}
    MeroLiveClose(1);
    puts("PASS: fragmented CE packets, weather dispatch, cinema wake, disconnect recovery, tick wrap");
    return 0;
}
