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

    /* Allow the SD driver to mount after cold boot. Keep flash recovery untouched. */
    {
        int attempt;
        for (attempt = 0; attempt < 10; attempt++) {
            if (GetFileAttributesW(L"\\SDMMC\\MERO\\mero-shell.exe") != 0xFFFFFFFF) break;
            Sleep(500);
        }
    }

    /* Try SDMMC first (latest version), fallback to ResidentFlash */
    if (!CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        if (!CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
            if (!CreateProcessW(L"\\Windows\\Launch.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi) &&
                !CreateProcessW(L"\\Windows\\explorer.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi))
            MessageBoxW(NULL,
                L"Mero Shell not found.\nIf device is in USB Mass Storage mode, dismount USB or tap USB Storage Mode.",
                L"Mero Launcher", MB_OK | MB_ICONWARNING);
        }
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return 0;
}
