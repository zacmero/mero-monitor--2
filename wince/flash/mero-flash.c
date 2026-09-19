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

    /* If SDMMC is available, keep ResidentFlash updated */
    if (GetFileAttributesW(L"\\SDMMC\\MERO\\mero-shell.exe") != 0xFFFFFFFF) {
        CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
        SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-shell.exe", FILE_ATTRIBUTE_NORMAL);
        CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE);
    }

    /* Try SDMMC first (latest version), fallback to ResidentFlash */
    if (!CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        if (!CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
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
