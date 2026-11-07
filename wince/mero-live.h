/* Host telemetry and reversible cinema blanking. Never suspend the USB link. */
#ifndef MERO_LIVE_H
#define MERO_LIVE_H
#include <winsock2.h>
#include "mero-bus.h"
#ifdef MERO_AGENT_BUILD
#include "mero-cinema-backlight.h"
#endif
#define MERO_LIVE_TIMER 99
static SOCKET mero_socket = INVALID_SOCKET;
static BOOL mero_connecting = FALSE, mero_standby = FALSE;
static DWORD mero_attempt = 0, mero_received = 0, mero_session = 0;
static char mero_rx[8192];
static int mero_rx_len = 0;
static HWND mero_blank = NULL;
static void (*mero_line_handler)(char *) = NULL;
static DWORD mero_bus_generation;

static void MeroLiveDisconnect(void)
{
    if (mero_socket != INVALID_SOCKET) closesocket(mero_socket);
    mero_socket = INVALID_SOCKET;
    mero_connecting = FALSE;
    mero_rx_len = 0;
}

static LRESULT CALLBACK MeroBlankProc(HWND h, UINT message, WPARAM w, LPARAM l)
{
    if (message == WM_PAINT) {
        PAINTSTRUCT paint; RECT area;
        HDC dc = BeginPaint(h, &paint);
        GetClientRect(h, &area);
        FillRect(dc, &area, (HBRUSH)GetStockObject(BLACK_BRUSH));
        EndPaint(h, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, message, w, l);
}
static void MeroLiveStandby(HWND owner, BOOL standby)
{
#ifdef MERO_AGENT_BUILD
    MeroCinemaBacklight(standby);
#endif
    if (mero_standby == standby) return;
    mero_standby = standby;
    if (standby) {
        if (!mero_blank) {
            WNDCLASSW cls;
            memset(&cls, 0, sizeof(cls));
            cls.lpfnWndProc = MeroBlankProc;
            cls.hInstance = GetModuleHandleW(NULL);
            cls.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            cls.lpszClassName = L"MERO_STANDBY_BLACK_V1";
            RegisterClassW(&cls);
            mero_blank = CreateWindowExW(WS_EX_TOPMOST, cls.lpszClassName, L"",
                WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                GetSystemMetrics(SM_CYSCREEN), owner, NULL,
                GetModuleHandleW(NULL), NULL);
        }
        if (mero_blank) ShowWindow(mero_blank, SW_SHOW);
        else mero_standby = FALSE;
    } else {
        if (mero_blank) ShowWindow(mero_blank, SW_HIDE);
        InvalidateRect(owner, NULL, FALSE);
    }
}

static void MeroLiveTick(HWND owner)
{
    DWORD now = GetTickCount();
    char shared[MERO_BUS_BYTES];
    shared[0] = 0;
    if (MeroBusLock()) {
        if (mero_bus->generation != mero_bus_generation &&
            (DWORD)(now - mero_bus->tick) < 15000) {
            mero_bus_generation = mero_bus->generation;
            memcpy(shared, mero_bus->lines, sizeof(shared)); shared[sizeof(shared)-1] = 0;
            mero_received = now;
        }
        ReleaseMutex(mero_bus_mutex);
    }
    if (shared[0]) {
        char *line = shared, *end;
        while ((end = strchr(line, '\n')) != NULL) {
            *end = 0;
            if (!strcmp(line, "MERO:POWER:standby=1")) MeroLiveStandby(owner, TRUE);
            else if (!strcmp(line, "MERO:POWER:standby=0")) MeroLiveStandby(owner, FALSE);
            else if (mero_line_handler) mero_line_handler(line);
            line = end + 1;
        }
    }
    if (mero_standby && (DWORD)(now - mero_received) > 15000)
        MeroLiveStandby(owner, FALSE);
    if (mero_socket == INVALID_SOCKET) {
        struct sockaddr_in addr;
        u_long nonblocking = 1;
        int result, error;
        if (mero_attempt && (DWORD)(now - mero_attempt) < 3000) return;
        mero_attempt = now;
        mero_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (mero_socket == INVALID_SOCKET) return;
        ioctlsocket(mero_socket, FIONBIO, &nonblocking);
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(5000);
        addr.sin_addr.s_addr = inet_addr("192.168.55.101");
        result = connect(mero_socket, (struct sockaddr *)&addr, sizeof(addr));
        error = WSAGetLastError();
        if (result == SOCKET_ERROR && error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
            MeroLiveDisconnect();
            return;
        }
        mero_connecting = (result == SOCKET_ERROR);
        mero_session = now;
    }
    if (mero_connecting) {
        fd_set writefds, errors;
        struct timeval timeout = {0, 0};
        FD_ZERO(&writefds); FD_ZERO(&errors);
        FD_SET(mero_socket, &writefds); FD_SET(mero_socket, &errors);
        if (select(0, NULL, &writefds, &errors, &timeout) <= 0) {
            if ((DWORD)(now - mero_attempt) > 5000) MeroLiveDisconnect();
            return;
        }
        if (FD_ISSET(mero_socket, &errors)) { MeroLiveDisconnect(); return; }
        mero_connecting = FALSE;
    }
    for (;;) {
        int count = recv(mero_socket, mero_rx + mero_rx_len,
                         sizeof(mero_rx) - 1 - mero_rx_len, 0);
        char *newline;
        if (count == 0) { MeroLiveDisconnect(); break; }
        if (count == SOCKET_ERROR) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) MeroLiveDisconnect();
            break;
        }
        mero_received = now;
        mero_session = now;
        mero_rx_len += count;
        mero_rx[mero_rx_len] = 0;
        while ((newline = strchr(mero_rx, '\n')) != NULL) {
            int consumed = (int)(newline - mero_rx) + 1;
            *newline = 0;
            if (strcmp(mero_rx, "MERO:POWER:standby=1") == 0) MeroLiveStandby(owner, TRUE);
            else if (strcmp(mero_rx, "MERO:POWER:standby=0") == 0) MeroLiveStandby(owner, FALSE);
            else if (mero_line_handler) mero_line_handler(mero_rx);
            mero_rx_len -= consumed;
            memmove(mero_rx, mero_rx + consumed, mero_rx_len);
            mero_rx[mero_rx_len] = 0;
        }
        if (mero_rx_len == sizeof(mero_rx) - 1) { MeroLiveDisconnect(); break; }
    }
    if (mero_socket != INVALID_SOCKET && (DWORD)(now - mero_session) > 15000)
        MeroLiveDisconnect();
    /* The recovery timeout runs before reconnect early returns. */
}

static void MeroLiveInit(HWND owner, void (*handler)(char *))
{
    WSADATA data;
    mero_line_handler = handler;
    MeroBusOpen();
    if (WSAStartup(MAKEWORD(2, 0), &data) == 0)
        SetTimer(owner, MERO_LIVE_TIMER, 500, NULL);
}

static void MeroLiveClose(HWND owner)
{
#ifdef MERO_AGENT_BUILD
    MeroCinemaBacklight(0);
#endif
    KillTimer(owner, MERO_LIVE_TIMER);
    MeroLiveDisconnect();
    if (mero_blank) DestroyWindow(mero_blank);
    mero_blank = NULL;
    MeroBusClose();
    WSACleanup();
}
#endif
