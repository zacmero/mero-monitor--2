/*
 * mero-flash.c - Resident Flash Shell Forwarder with Amber Cybernetic Icon
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR    lpCmdLine,
    int       nCmdShow)
{
    PROCESS_INFORMATION pi;
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    memset(&pi, 0, sizeof(pi));
    /* Launch ResidentFlash shell; fallback to SDMMC if resident copy missing */
    if (!CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return 0;
}
