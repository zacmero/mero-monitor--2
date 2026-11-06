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
    MeroLiveClose(1);
    puts("PASS: fragmented CE packets, weather dispatch, cinema wake, disconnect recovery, tick wrap");
    return 0;
}
