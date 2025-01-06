#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include "defines.h"
#include "platform.h"

#include "renderer/vk_renderer.cpp"

global_var bool running = true;
global_var HWND window = 0;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam,  LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        running = false;
        break;
    }
    return DefWindowProc(window, msg, wParam, lParam);
}

bool platform_create_window() {

    // Register the window class.
    const wchar_t CLASS_NAME[]  = L"vulkan_engine_class";

    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS wc = {};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if(!RegisterClass(&wc)){
        MessageBoxA(0, "Couldn't Register window Class", "error", MB_OK | MB_ICONEXCLAMATION);
        return false;
    };

    window = CreateWindowEx(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        L"I love Pong",
        WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
        100, 100, 1600, 720, 0, 0, instance, 0
    );

    if (window == 0) {
        MessageBoxA(0, "Couldn't Create Window", "error", MB_OK | MB_ICONEXCLAMATION);
        return false;
    }

    ShowWindow(window, SW_SHOW);

    return true;
}

void platform_update_window (HWND window) {
    MSG msg;
    while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int main () {
    VkContext vkcontext = {};
    if (!platform_create_window())
    {
        return -1;
    }
    
    if (!vk_init(&vkcontext, window)) {
        return -1;
    }

    while (running)
    {
        platform_update_window(window);
        if (!vk_render(&vkcontext))
        {
            return -1;
        }
    };
    

    return 0;
}

void platform_get_window_size (uint32_t* width, uint32_t* height) {
    RECT rect;
    GetClientRect(window, &rect);

    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}