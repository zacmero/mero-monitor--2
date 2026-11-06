#ifndef MERO_SHELL_RETURN_H
#define MERO_SHELL_RETURN_H
/* App exits suppress the saved startup app: returning home must not loop. */
static void MeroReturnToShell(void) {
    HWND shell = FindWindowW(L"MeroShellWndClass", NULL);
    PROCESS_INFORMATION pi;
    if (shell) {
        PostMessageW(shell, RegisterWindowMessageW(L"MERO_SHELL_HOME_V1"), 0, 0);
        ShowWindow(shell, SW_SHOW); SetForegroundWindow(shell); return;
    }
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", L"--home", NULL,NULL,FALSE,0,NULL,NULL,NULL,&pi))
        CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", L"--home",NULL,NULL,FALSE,0,NULL,NULL,NULL,&pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);
}
#endif
