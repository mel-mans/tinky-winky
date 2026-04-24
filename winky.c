#include <windows.h>
#include <stdio.h>

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lParam;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main(void)
{
    HHOOK hook = SetWindowsHookEx(
        WH_KEYBOARD_LL,         
        LowLevelKeyboardProc,   
        GetModuleHandle(NULL),  
        0                      
    );

    if (!hook)
    {
        fprintf(stderr, "Failed to set hook: %lu\n", GetLastError());
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hook);
    return 0;
}