/* Hardware acceptance helper: press only the Music deck's Play/Pause button.
 * No general UI injection or system-window targeting. Run twice to restore pause. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPWSTR args,int show) {
#ifdef TEST_CLOSE_SENSOR
    HWND sensor=FindWindowW(L"MERO_COMPANION_DECK",NULL);
    (void)a;(void)b;(void)args;(void)show;
    if(!sensor)return 1;
    PostMessageW(sensor,WM_CLOSE,0,0);return 0;
#else
    HWND music=FindWindowW(L"MERO_MUSIC",NULL);
    (void)a;(void)b;(void)args;(void)show;
    if(!music)return 1;
    PostMessageW(music,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(230,205));
    PostMessageW(music,WM_LBUTTONUP,0,MAKELPARAM(230,205));
    return 0;
#endif
}
