#include "../include/pch.hpp"
#include "../include/octolite_window.hpp"

namespace octolite {

namespace {
    LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

OctoWindow create_window(uint32_t width, uint32_t height, const wchar_t* title) {
    OctoWindow win;
    win.width = width;
    win.height = height;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"octolite_window";
    // DÜZELTME: IDC_ARROW makrosu yerine MAKEINTRESOURCEW kullanıldı
    // IDC_ARROW = 32512 (WinUser.h'da tanımlı)
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassExW(&wc);

    RECT r{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    win.hwnd = CreateWindowExW(
        0,
        L"octolite_window",
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );
    ShowWindow(win.hwnd, SW_SHOW);
    return win;
}

void destroy_window(OctoWindow& win) {
    if (win.hwnd) DestroyWindow(win.hwnd);
    win = {};
}

bool pump_messages(OctoWindow& win) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { win.quit = true; return false; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !win.quit;
}

} // namespace octolite
