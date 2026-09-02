#include "stdafx.h"

namespace Utils
{

std::wstring GetWindowText(HWND hwnd)
{
    const int len = max(GetWindowTextLengthW(hwnd), MAX_PATH);
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    s.resize(static_cast<size_t>(GetWindowTextW(hwnd, &s[0], len + 1)));
    return s;
}

bool IsWindowCloaked(HWND hwnd)
{
    BOOL cloaked = FALSE;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(hr) && cloaked;
}

HWND GetAltTabRepresentative(HWND hwnd)
{
    return GetAncestor(hwnd, GA_ROOTOWNER);
}

HICON GetSmallWindowIcon(HWND hwnd)
{
    const int timeout = 25;
    DWORD_PTR lr = 0;
    SendMessageTimeout(hwnd, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, timeout, &lr);
    if (!lr)
        lr = GetClassLongPtrW(hwnd, GCLP_HICON);
    if (!lr)
        SendMessageTimeout(hwnd, WM_GETICON, ICON_SMALL, 0, SMTO_ABORTIFHUNG, timeout, &lr);
    if (!lr)
        lr = GetClassLongPtrW(hwnd, GCLP_HICONSM);
    if (!lr)
        SendMessageTimeout(hwnd, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, timeout, &lr);

    return lr ? CopyIcon(reinterpret_cast<HICON>(lr)) : nullptr;
}

bool IsButtonDown(const int vKey)
{
    return (GetAsyncKeyState(vKey) & 0x8000) != 0;
}

bool IsAltDown()
{
    return IsButtonDown(VK_MENU);
}

bool IsShiftDown()
{
    return IsButtonDown(VK_SHIFT);
}

bool IsReallyMinimized(HWND hwnd, LPRECT pRectRestored)
{
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd, &wp))
    {
        *pRectRestored = wp.rcNormalPosition;
        return IsIconic(hwnd) || wp.showCmd == SW_SHOWMINIMIZED || wp.showCmd == SW_MINIMIZE || wp.showCmd == SW_SHOWMINNOACTIVE;
    }

    return false;
}

bool PtInRect(const RECT& rectTarget, const RECT& rectOrigin)
{
    return ::PtInRect(&rectTarget, POINT{ rectOrigin.left + (rectOrigin.right - rectOrigin.left) / 2, rectOrigin.top + (rectOrigin.bottom - rectOrigin.top) / 2 });
}

void ForceAltKeyUp()
{
    INPUT inputs[3]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU;
    inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_LMENU;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_RMENU;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(3, inputs, sizeof(INPUT));
}

void ForceAltKeyDown()
{
    INPUT inputs[1]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU;
    SendInput(1, inputs, sizeof(INPUT));
}

HMONITOR GetActiveMonitorFromCursor()
{
    POINT ptCursor = { 0 };
    GetCursorPos(&ptCursor);
    return MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
}

RECT GetActiveMonitorRect()
{
    MONITORINFO mi = { 0 };
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(GetActiveMonitorFromCursor(), &mi);
    return mi.rcMonitor;
}

int GetProfileInt(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const int defaultValue)
{
    return GetPrivateProfileIntW(section, name, defaultValue, ini);
}

bool GetProfileBool(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const bool defaultValue)
{
    const int val = GetPrivateProfileIntW(section, name, defaultValue, ini);
    return val != 0;
}

std::wstring GetProfileString(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const std::wstring& defaultValue)
{
    WCHAR buf[MAX_PATH] = {0};
    const auto length = GetPrivateProfileStringW(section, name, defaultValue.c_str(), buf, _countof(buf), ini);
    return std::wstring(buf, length);
}

bool SetProfileInt(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const int value)
{
    wchar_t buf[16]{};
    swprintf_s(buf, L"%d", value);
    return WritePrivateProfileStringW(section, name, buf, ini);
}

bool SetProfileString(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const std::wstring& value)
{
    return WritePrivateProfileStringW(section, name, value.c_str(), ini);
}

} // namespace Utils
