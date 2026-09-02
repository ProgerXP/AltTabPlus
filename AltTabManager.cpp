#include "stdafx.h"
#include "AltTabManager.h"
#include "resource.h"
#include <sstream>
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

// #NOTE: enable EM_SETCUEBANNER support
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static constexpr LPCWSTR MAIN_WINDOW_NAME = L"AltTabPlus Host";
static constexpr LPCWSTR TOOLBAR_HINT = L"Alt+Tab Plus";

static const UINT WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");
static constexpr UINT WM_TRAYICON = WM_APP + 1;
static constexpr UINT WM_APP_ACTIVATE_SELECTION = WM_APP + 2;
static constexpr UINT WM_APP_FOCUS_SEARCH = WM_APP + 3;
static constexpr UINT WM_APP_SHOW_SWITCHER = WM_APP + 4;

static constexpr UINT TRAY_UID = 1;
static constexpr int ID_SEARCH_EDIT = 2001;

CAltTabManager::CAltTabManager(HINSTANCE hInstance)
    : m_hInstance(hInstance)
    , m_columns(L"Columns", INI_UI_SECTION, 7, [](const int& v)->int { return min(max(1, v), 20); })
    , m_ignoreMinimized(L"IgnoreMinimized", INI_FEATURES_SECTION, true)
    , m_autoResizeOnFilterChange(L"AutoResizeOnFilterChange", INI_UI_SECTION, false)
    , m_iconSize(L"IconSize", INI_UI_SECTION, 32)
    , m_itemWidth(L"ItemWidth", INI_UI_SECTION, 40)
    , m_itemHeight(L"ItemHeight", INI_UI_SECTION, 40)
    , m_gapX(L"GapX", INI_UI_SECTION, 10)
    , m_gapY(L"GapY", INI_UI_SECTION, 10)
    , m_gridTop(L"GridTop", INI_UI_SECTION, 44)
    , m_gridMarginY(L"GridMarginY", INI_UI_SECTION, 4)
    , m_outerMargin(L"OuterMargin", INI_UI_SECTION, 10)

    , m_borderColor(L"BorderColor", INI_COLORS_SECTION, RGB(24, 129, 215))
    , m_borderSize(L"BorderSize", INI_UI_SECTION, 2)
    , m_textColor(L"TextColor", INI_COLORS_SECTION, RGB(0, 0, 0))
    , m_backgroundColor(L"BackgroundColor", INI_COLORS_SECTION, RGB(240, 240, 240))
    , m_selectedItemBackgroundColor(L"SelectedItemBackgroundColor", INI_COLORS_SECTION, RGB(255, 255, 255))
    , m_selectedItemBorderColor(L"SelectedItemBorderColor", INI_COLORS_SECTION, RGB(24, 129, 215))
    , m_selectedItemBorderWidth(L"SelectedItemBorderSize", INI_UI_SECTION, 2)
    , m_itemColorNoIcon(L"ItemColorNoIcon", INI_COLORS_SECTION, RGB(90, 90, 90))
    , m_fontSize(L"FontSize", INI_UI_SECTION, 18)
    , m_fontFaceName(L"FontFaceName", INI_UI_SECTION, L"Segoe UI")

    , m_searchTop(L"SearchTop", INI_UI_SECTION, 10)
    , m_searchHeight(L"SearchHeight", INI_UI_SECTION, 24)
    , m_searchMarginX(L"SearchMarginX", INI_UI_SECTION, 8)
    , m_searchMarginY(L"SearchMarginY", INI_UI_SECTION, 8)
    , m_searchFontSize(L"SearchFontSize", INI_UI_SECTION, 18)
    , m_searchFontFaceName(L"SearchFontFaceName", INI_UI_SECTION, L"Segoe UI")
    , m_searchTextColor(L"SearchTextColor", INI_COLORS_SECTION, RGB(0, 0, 0))
    , m_searchBackgroundColor(L"SearchBackgroundColor", INI_COLORS_SECTION, RGB(255, 255, 255))

    , m_bottomPadding(L"BottomPadding", INI_UI_SECTION, 10)
    , m_selectedTitleHeight(L"SelectedTitleHeight", INI_UI_SECTION, 33)
    , m_maxMenuHeight(L"MaxMenuHeight", INI_UI_SECTION, 520)
    , m_minMenuWidth(L"MinMenuWidth", INI_UI_SECTION, 220)
    , m_minMenuHeight(L"MinMenuHeight", INI_UI_SECTION, 140)

    , m_minCursorOffsetForItemSelection(L"MinCursorOffsetForItemSelection", INI_UI_SECTION, 10)
    , m_showActiveMonitorWindowsOnly(L"ShowActiveMonitorWindowsOnly", INI_FEATURES_SECTION, true)
    , m_showTrayIcon(L"ShowTrayIcon", INI_FEATURES_SECTION, true)
    , m_hideOnMisclick(L"HideOnMisclick", INI_FEATURES_SECTION, false)
    , m_windowActivationMethod(L"WindowActivationMethod", INI_FEATURES_SECTION, 0, [](const int& v)->int { return min(max(0, v), 2); })
{
    s_instance = this;
    InitializeIniPath();
}

CAltTabManager::~CAltTabManager()
{
    Cleanup();

    s_instance = nullptr;
}

int CAltTabManager::Run()
{
    if (!AcquireSingleInstance())
        return 0;

    if (!RegisterClasses())
        return 1;
    if (!CreateWindows())
        return 2;

    LoadSettings();

    if (!InstallHooks())
        return 3;

    const HACCEL c_hAccSearchInline = LoadAccelerators(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_ACC_SEARCH_INLINE));

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (IsWindow(m_hwndSearch) && (msg.hwnd == m_hwndSearch))
        {
            if (TranslateAccelerator(msg.hwnd, c_hAccSearchInline, &msg))
                continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

void CAltTabManager::InitializeIniPath()
{
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::wstring path = modulePath;
    size_t dot = path.find_last_of(L'.');
    if (dot != std::wstring::npos)
        path.resize(dot);

    path += L".ini";
    m_iniPath = path;
}

void CAltTabManager::EnsureIniExists() const
{
    const auto path = m_iniPath.c_str();
    const HANDLE hFile = CreateFile(path, OPEN_ALWAYS, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
            WriteFile(hFile, INI_HEADER, wcslen(INI_HEADER) * sizeof(WCHAR), NULL, NULL);
        CloseHandle(hFile);
    }
    m_columns.save(path);
    m_ignoreMinimized.save(path);
    m_autoResizeOnFilterChange.save(path);
    m_iconSize.save(path);
    m_itemWidth.save(path);
    m_itemHeight.save(path);
    m_gapX.save(path);
    m_gapY.save(path);
    m_gridTop.save(path);
    m_gridMarginY.save(path);
    m_outerMargin.save(path);

    m_borderColor.save(path);
    m_borderSize.save(path);
    m_textColor.save(path);
    m_backgroundColor.save(path);
    m_selectedItemBackgroundColor.save(path);
    m_selectedItemBorderColor.save(path);
    m_selectedItemBorderWidth.save(path);
    m_itemColorNoIcon.save(path);
    m_fontSize.save(path);
    m_fontFaceName.save(path);

    m_searchTop.save(path);
    m_searchHeight.save(path);
    m_searchMarginX.save(path);
    m_searchMarginY.save(path);
    m_searchFontSize.save(path);
    m_searchFontFaceName.save(path);
    m_searchTextColor.save(path);
    m_searchBackgroundColor.save(path);

    m_bottomPadding.save(path);
    m_selectedTitleHeight.save(path);
    m_maxMenuHeight.save(path);
    m_minMenuWidth.save(path);
    m_minMenuHeight.save(path);

    m_minCursorOffsetForItemSelection.save(path);
    m_showActiveMonitorWindowsOnly.save(path);
    m_showTrayIcon.save(path);
    m_hideOnMisclick.save(path);
    m_windowActivationMethod.save(path);
}

void CAltTabManager::OpenIniInNotepad()
{
    EnsureIniExists();
    ShellExecuteW(m_hwndMain, L"open", m_iniPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void CAltTabManager::LoadSettings()
{
    const auto path = m_iniPath.c_str();
    m_columns.load(path);
    m_ignoreMinimized.load(path);
    m_autoResizeOnFilterChange.load(path);
    m_iconSize.load(path);
    m_itemWidth.load(path);
    m_itemHeight.load(path);
    m_gapX.load(path);
    m_gapY.load(path);
    m_gridTop.load(path);
    m_gridMarginY.load(path);
    m_outerMargin.load(path);

    m_borderColor.load(path);
    m_borderSize.load(path);
    m_textColor.load(path);
    m_backgroundColor.load(path);
    m_selectedItemBackgroundColor.load(path);
    m_selectedItemBorderColor.load(path);
    m_selectedItemBorderWidth.load(path);
    m_itemColorNoIcon.load(path);
    m_fontSize.load(path);
    m_fontFaceName.load(path);

    m_searchTop.load(path);
    m_searchHeight.load(path);
    m_searchMarginX.load(path);
    m_searchMarginY.load(path);
    m_searchFontSize.load(path);
    m_searchFontFaceName.load(path);
    m_searchTextColor.load(path);
    m_searchBackgroundColor.load(path);

    m_bottomPadding.load(path);
    m_selectedTitleHeight.load(path);
    m_maxMenuHeight.load(path);
    m_minMenuWidth.load(path);
    m_minMenuHeight.load(path);
    
    m_minCursorOffsetForItemSelection.load(path);
    m_showActiveMonitorWindowsOnly.load(path);
    m_showTrayIcon.load(path);
    m_hideOnMisclick.load(path);
    m_windowActivationMethod.load(path);

    m_brushSearchBackground.attach(CreateSolidBrush(m_searchBackgroundColor));
    m_penBorder.attach(CreatePen(PS_INSIDEFRAME, m_borderSize, m_borderColor));
    m_brushBackground.attach(CreateSolidBrush(m_backgroundColor));
    m_brushSelectedItem.attach(CreateSolidBrush(m_selectedItemBackgroundColor));
    m_penSelectedItem.attach(CreatePen(PS_INSIDEFRAME, m_selectedItemBorderWidth, m_selectedItemBorderColor));
    m_brushItemNoIcon.attach(CreateSolidBrush(m_itemColorNoIcon));

    m_font.attach(CreateFontW(
        m_fontSize, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, m_fontFaceName.value().c_str()
    ));

    m_fontSearch.attach(CreateFontW(
        m_searchFontSize, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, m_searchFontFaceName.value().c_str()
    ));

    SendMessageW(m_hwndSearch, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontSearch.handle()), TRUE);

    if (m_showTrayIcon)
        CreateTrayIcon();
    else
        RemoveTrayIcon();
}

bool CAltTabManager::AcquireSingleInstance()
{
    m_hSingleInstanceMutex = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\AltTabPlus_SingleInstance_Mutex_64483DFA"
    );

    if (!m_hSingleInstanceMutex)
        return false;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(m_hSingleInstanceMutex);
        m_hSingleInstanceMutex = nullptr;
        return false;
    }

    return true;
}

bool CAltTabManager::ShouldForceAltUp(CloseReason reason) const
{
    switch (reason)
    {
    case CloseReason::ActivateByAltRelease:
    case CloseReason::SearchMode:
    case CloseReason::Other:
        return true;

    case CloseReason::Escape:
    case CloseReason::FocusLost:
    case CloseReason::ClickOutside:
    case CloseReason::ActivateByEnterWhileAltHeld:
    case CloseReason::ActivateByEnterWithoutAlt:
    case CloseReason::None:
    default:
        return false;
    }
}

int CAltTabManager::GetTotalRows() const
{
    return (static_cast<int>(m_items.size()) + m_columns - 1) / m_columns;
}

int CAltTabManager::GetVisibleRowsForHeight(int height) const
{
    const int top = m_gridTop;
    const int bottom = height - (m_selectedTitleHeight + m_bottomPadding + m_gridMarginY * 2);
    const int available = bottom - top;
    const int step = m_itemHeight + m_gapY;
    return (available + m_gapY) / step;
}

int CAltTabManager::GetVisibleRows() const
{
    RECT client{};
    if (!GetClientRect(m_hwndSwitcher, &client))
        return 1;
    return GetVisibleRowsForHeight(client.bottom);
}

int CAltTabManager::GetVisibleScrollBarWidth() const
{
    const bool scrollBarVisible = GetTotalRows() > GetVisibleRows();
    return scrollBarVisible ? GetSystemMetrics(SM_CXVSCROLL) : 0;
}

RECT CAltTabManager::GetContentAreaRect() const
{
    RECT client{};
    if (!GetClientRect(m_hwndSwitcher, &client))
    {
        RECT fallback{ m_outerMargin, 0, m_outerMargin + 1, 0 };
        return fallback;
    }

    RECT rc{};
    rc.left = m_outerMargin;
    rc.top = 0;
    rc.right = client.right - m_outerMargin;
    rc.bottom = client.bottom;
    return rc;
}

RECT CAltTabManager::GetSearchRect() const
{
    RECT contentRc = GetContentAreaRect();
    RECT rc{
        contentRc.left,
        m_searchTop,
        contentRc.right,
        m_searchTop + m_searchHeight
    };
    return rc;
}

BOOL CALLBACK CAltTabManager::EnumWindowsFunc(HWND hwnd, LPARAM lParam)
{
    auto* self = reinterpret_cast<CAltTabManager*>(lParam);
    return self->OnEnumWindow(hwnd);
}

LRESULT CALLBACK CAltTabManager::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (s_instance)
        return s_instance->OnKeyboard(code, wParam, lParam);
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK CAltTabManager::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<CAltTabManager*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<CAltTabManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->MainWndProcImpl(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CAltTabManager::SwitcherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = reinterpret_cast<CAltTabManager*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<CAltTabManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->SwitcherWndProcImpl(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CAltTabManager::SearchEditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<CAltTabManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return self->SearchEditWndProcImpl(hwnd, msg, wParam, lParam);
}

bool CAltTabManager::RegisterClasses()
{
    WNDCLASSEXW wcMain{};
    wcMain.cbSize = sizeof(wcMain);
    wcMain.lpfnWndProc = MainWndProc;
    wcMain.hInstance = m_hInstance;
    wcMain.lpszClassName = MAIN_CLASS_NAME;
    wcMain.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wcMain))
        return false;

    WNDCLASSEXW wcSwitcher{};
    wcSwitcher.cbSize = sizeof(wcSwitcher);
    wcSwitcher.lpfnWndProc = SwitcherWndProc;
    wcSwitcher.hInstance = m_hInstance;
    wcSwitcher.lpszClassName = SWITCHER_CLASS_NAME;
    wcSwitcher.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wcSwitcher))
        return false;

    return true;
}

bool CAltTabManager::CreateSearchControl()
{
    const RECT rc = GetSearchRect();

    m_hwndSearch = CreateWindowExW(
        0,
        WC_EDIT,
        L"",
        WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        m_hwndSwitcher,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SEARCH_EDIT)),
        m_hInstance,
        nullptr
    );

    if (!m_hwndSearch)
        return false;

    SendMessageW(m_hwndSearch, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontSearch.handle()), TRUE);
    SendMessageW(m_hwndSearch, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(m_searchMarginX, m_searchMarginY));
    SendMessageW(m_hwndSearch, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Use Alt+Backtick to filter..."));

    SetWindowLongPtrW(m_hwndSearch, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_originalSearchProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(m_hwndSearch, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SearchEditWndProc))
        );

    return true;
}

void CAltTabManager::LayoutSearchControl()
{
    if (!m_hwndSearch)
        return;

    const RECT rc = GetSearchRect();
    MoveWindow(
        m_hwndSearch,
        rc.left,
        rc.top,
        rc.right - rc.left,
        rc.bottom - rc.top,
        TRUE
    );
}

bool CAltTabManager::CreateWindows()
{
    m_hwndMain = CreateWindowExW(
        0, MAIN_CLASS_NAME, MAIN_WINDOW_NAME, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
        nullptr, nullptr, m_hInstance, this
    );
    if (!m_hwndMain)
        return false;

    ShowWindow(m_hwndMain, SW_HIDE);

    m_hwndSwitcher = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        SWITCHER_CLASS_NAME,
        L"",
        WS_POPUP | WS_VSCROLL | WS_CLIPCHILDREN,
        0, 0, 900, 430,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwndSwitcher)
        return false;

    if (!CreateSearchControl())
        return false;

    return true;
}

bool CAltTabManager::CreateTrayIcon()
{
    if (!m_showTrayIcon)
        return true;

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwndMain;
    m_nid.uID = TRAY_UID;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = reinterpret_cast<HICON>(
        LoadImageW(
            m_hInstance,
            MAKEINTRESOURCE(IDI_MAIN),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            0
        )
        );
    if (!m_nid.hIcon)
        m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    lstrcpynW(m_nid.szTip, TOOLBAR_HINT, ARRAYSIZE(m_nid.szTip));
    return Shell_NotifyIconW(NIM_ADD, &m_nid) == TRUE;
}

void CAltTabManager::RemoveTrayIcon()
{
    if (m_nid.hWnd)
        Shell_NotifyIconW(NIM_DELETE, &m_nid);

    if (m_nid.hIcon)
    {
        DestroyIcon(m_nid.hIcon);
        m_nid.hIcon = nullptr;
    }
}

bool CAltTabManager::InstallHooks()
{
    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, m_hInstance, 0);
    if (!m_keyboardHook)
        return false;

    return m_keyboardHook != nullptr;
}

void CAltTabManager::Cleanup()
{
    RemoveTrayIcon();
    FreeItemIcons();

    if (m_keyboardHook)
    {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }

    if (m_hSingleInstanceMutex)
    {
        CloseHandle(m_hSingleInstanceMutex);
        m_hSingleInstanceMutex = nullptr;
    }
}

void CAltTabManager::FreeItemIcons()
{
    for (auto& item : m_items)
    {
        if (item.icon)
        {
            DestroyIcon(item.icon);
            item.icon = nullptr;
        }
    }
}

IsAltTabCandidateResult CAltTabManager::IsAltTabCandidate(HWND hwnd, const RECT& rectMonitor) const
{
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return IsAltTabCandidateResult::NotWindowOrNotVisible;
    RECT rectRestored = {};
    const bool isMinimized = Utils::IsReallyMinimized(hwnd, &rectRestored);
    if (m_ignoreMinimized && isMinimized)
        return IsAltTabCandidateResult::SkipMinimized;

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (style & WS_CHILD)
        return IsAltTabCandidateResult::SkipChild;
    if (exStyle & WS_EX_TOOLWINDOW)
        return IsAltTabCandidateResult::SkipTool;
    if (Utils::IsWindowCloaked(hwnd))
        return IsAltTabCandidateResult::SkipCloaked;

    HWND owner = GetWindow(hwnd, GW_OWNER);
    RECT rcOwner = {};
    if (owner)
        GetWindowRect(owner, &rcOwner);
    if (owner != nullptr && !(exStyle & WS_EX_APPWINDOW) && !IsRectEmpty(&rcOwner))
        return IsAltTabCandidateResult::SkipOwned;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc))
        return IsAltTabCandidateResult::SkipFailedGetWindowRect;
    if (IsRectEmpty(&rc))
        return IsAltTabCandidateResult::SkipEmptyRect;

    return (Utils::PtInRect(rectMonitor, rc) || (!m_ignoreMinimized && Utils::PtInRect(rectMonitor, rectRestored)))
        ? IsAltTabCandidateResult::Success
        : IsAltTabCandidateResult::SkipNotInRect;
}

BOOL CAltTabManager::OnEnumWindow(HWND hwnd)
{
    HWND rep = Utils::GetAltTabRepresentative(hwnd);
    if (rep != hwnd)
        return TRUE;

    const auto reason = IsAltTabCandidate(hwnd, m_rectMonitorForWindowsEnumeration);
    if (reason == IsAltTabCandidateResult::Success)
        m_mru.push_back(hwnd);
    else
        m_mapEnumFailures[hwnd] = reason;

    return TRUE;
}

void CAltTabManager::BuildWindowList(const bool reload)
{
    if (reload)
    {
        m_mru.clear();
        EnumWindows(EnumWindowsFunc, reinterpret_cast<LPARAM>(this));
        FreeItemIcons();

        std::wstringstream ss;
        ss << INI_UI_SECTION << "> EnumWindows result:" << std::endl;
        for (auto f : m_mapEnumFailures)
            ss << std::hex << (LONG_PTR)f.first << "> " << std::dec << (int)f.second << std::endl;
        ss << INI_UI_SECTION << "> EnumWindows result finished" << std::endl;
        OutputDebugString(ss.str().c_str());
        m_mapEnumFailures.clear();
    }
    m_items.clear();

    for (const HWND hwnd : m_mru)
    {
        const HWND rep = Utils::GetAltTabRepresentative(hwnd);
        if (rep != hwnd)
            continue;

        const std::wstring title = Utils::GetWindowText(hwnd);
        if (!m_searchText.empty() && !StrStrIW(title.c_str(), m_searchText.c_str()))
            continue;

        AltTabItem item;
        item.hwnd = hwnd;
        item.title = title;
        item.icon = Utils::GetSmallWindowIcon(hwnd);
        m_items.push_back(std::move(item));
    }
}

SIZE CAltTabManager::CalculateSwitcherSize() const
{
    const int rows = GetTotalRows();
    const int cols = m_columns;

    const int scrollW = GetVisibleScrollBarWidth();
    const int contentWidth = cols * m_itemWidth + (cols - 1) * m_gapX;

    int width = m_outerMargin * 2 + contentWidth + scrollW;
    if (width < m_minMenuWidth)
        width = m_minMenuWidth;

    int height = m_gridTop + rows * m_itemHeight + (rows - 1) * m_gapY + m_gridMarginY * 2 + m_selectedTitleHeight + m_bottomPadding;
    if (height < m_minMenuHeight)
        height = m_minMenuHeight;
    if (height > m_maxMenuHeight)
        height = m_maxMenuHeight;

    SIZE s{};
    s.cx = width;
    s.cy = height;
    return s;
}

void CAltTabManager::UpdateScrollBar()
{
    const int totalRows = GetTotalRows();
    const int visibleRows = GetVisibleRows();

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;
    si.nMax = max(0, totalRows - 1);
    si.nPage = visibleRows;
    si.nPos = m_firstVisibleRow;
    SetScrollInfo(m_hwndSwitcher, SB_VERT, &si, TRUE);
}

void CAltTabManager::EnsureSelectionVisible()
{
    if (m_selectedIndex < 0)
        return;

    const int row = m_selectedIndex / m_columns;
    const int visibleRows = GetVisibleRows();

    if (row < m_firstVisibleRow)
        m_firstVisibleRow = row;
    else if (row >= m_firstVisibleRow + visibleRows)
        m_firstVisibleRow = max(0, row - visibleRows + 1);

    UpdateScrollBar();
}

void CAltTabManager::ActivateCurrentSelection(const CloseReason& reason)
{
    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_items.size()))
    {
        HideSwitcher(true, reason);
        return;
    }

    const HWND hwnd = m_items[m_selectedIndex].hwnd;
    if (IsWindow(hwnd))
    {
        if (ShouldForceAltUp(reason))
        {
            Utils::ForceAltKeyUp();
            m_altTabSession = false;
        }
        switch (m_windowActivationMethod)
        {
        case 0:
            SwitchToThisWindow(hwnd, TRUE);
            break;
        case 1:
            {
                const DWORD dwForegroundThread = GetWindowThreadProcessId(hwnd, NULL);
                const DWORD dwAppThread = GetCurrentThreadId();
                const BOOL bAttached = (dwForegroundThread != dwAppThread) ? AttachThreadInput(dwAppThread, dwForegroundThread, TRUE) : FALSE;
                const BOOL isMinimized = (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_MINIMIZE) == WS_MINIMIZE;
                ::ShowWindow(hwnd, isMinimized ? SW_RESTORE : SW_SHOW);
                ::SetForegroundWindow(hwnd);
                if (bAttached)
                    AttachThreadInput(dwAppThread, dwForegroundThread, FALSE);
            }
            break;
        case 2:
            if (IsIconic(hwnd))
                ShowWindow(hwnd, SW_RESTORE);
            INPUT inputs[1] = { { INPUT_MOUSE, {0} } };
            SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
            SetForegroundWindow(hwnd);
            break;
        }
    }
    HideSwitcher(false, reason);
}

void CAltTabManager::DeletePreviousWord()
{
    if (!m_hwndSearch)
        return;

    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(m_hwndSearch, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));

    if (start != end)
    {
        SendMessageW(m_hwndSearch, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
        return;
    }

    if (start == 0)
        return;

    std::wstring text = Utils::GetWindowText(m_hwndSearch);
    int pos = static_cast<int>(start);

    while (pos > 0 && iswspace(text[pos - 1]))
        --pos;

    while (pos > 0 && !iswspace(text[pos - 1]))
        --pos;

    SendMessageW(m_hwndSearch, EM_SETSEL, pos, start);
    SendMessageW(m_hwndSearch, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
}

void CAltTabManager::ShowSwitcher()
{
    LoadSettings();
    m_rectMonitorForWindowsEnumeration = m_showActiveMonitorWindowsOnly ? Utils::GetActiveMonitorRect() : RECT{};
    BuildWindowList(true);

    if (m_items.size() <= 1)
        return;

    m_selectedIndex = m_items.empty() ? -1 : 1;
    m_firstVisibleRow = 0;

    m_fixedSwitcherWidth = CalculateSwitcherSize().cx;
    UpdateWindowPos();
    LayoutSearchControl();

    UpdateScrollBar();
    InvalidateRect(m_hwndSwitcher, nullptr, TRUE);
    UpdateWindow(m_hwndSwitcher);
    SetForegroundWindow(m_hwndSwitcher);
    SetFocus(m_hwndSwitcher);
}

void CAltTabManager::HideSwitcher(const bool manageAltKey, const CloseReason reason)
{
    if (!IsActive() || m_hideSwitcherActive)
        return;

    m_hideSwitcherActive = true;
    if (manageAltKey && ShouldForceAltUp(reason))
    {
        Utils::ForceAltKeyUp();
        m_altDown = false;
        m_altTabSession = false;
    }
 
    ShowWindow(m_hwndSwitcher, SW_HIDE);

    m_selectedIndex = -1;
    m_firstVisibleRow = 0;
    m_searchText.clear();
    m_fixedSwitcherWidth = 0;
        
    m_internalSearchUpdate = true;
    SetWindowTextW(m_hwndSearch, L"");
    m_internalSearchUpdate = false;

    m_hideSwitcherActive = false;
}

void CAltTabManager::AdvanceSelection(const DWORD vkCode)
{
    if (!IsActive() || m_items.empty())
        return;

    const int n = static_cast<int>(m_items.size());
    switch (vkCode)
    {
    case VK_LEFT:
        m_selectedIndex = (m_selectedIndex - 1 + n) % n;
        break;
    case VK_RIGHT:
        m_selectedIndex = (m_selectedIndex + 1) % n;
        break;
    case VK_HOME:
        m_selectedIndex = m_items.empty() ? -1 : 0;
        break;
    case VK_END:
        m_selectedIndex = n - 1;
        break;
    case VK_UP:
        m_selectedIndex = m_selectedIndex - m_columns;
        break;
    case VK_DOWN:
        m_selectedIndex = m_selectedIndex + m_columns;
        break;
    }
    m_selectedIndex = max(0, min(n - 1, m_selectedIndex));
    EnsureSelectionVisible();
    InvalidateRect(m_hwndSwitcher, nullptr, TRUE);
}

void CAltTabManager::UpdateSearchAndRebuild()
{
    BuildWindowList(false);
    m_selectedIndex = m_items.empty() ? -1 : 0;
    m_firstVisibleRow = 0;

    if (IsActive())
    {
        if (m_autoResizeOnFilterChange)
        {
            UpdateWindowPos();
            LayoutSearchControl();
        }
        UpdateScrollBar();
    }

    InvalidateRect(m_hwndSwitcher, nullptr, TRUE);
    UpdateWindow(m_hwndSwitcher);
}

void CAltTabManager::UpdateWindowPos() const
{
    const bool isVisible = IsWindowVisible(m_hwndSwitcher);
    const SIZE size = CalculateSwitcherSize();
    const RECT monitorRect = Utils::GetActiveMonitorRect();
    const int x = monitorRect.left + (monitorRect.right - monitorRect.left - m_fixedSwitcherWidth) / 2;
    const int y = monitorRect.top + (monitorRect.bottom - monitorRect.top - size.cy) / 2;
    const int width = m_fixedSwitcherWidth > 0 ? m_fixedSwitcherWidth : size.cx;
    SetWindowPos(m_hwndSwitcher, isVisible ? nullptr : HWND_TOPMOST, x, y, width, size.cy, isVisible ? SWP_NOZORDER | SWP_NOACTIVATE : SWP_SHOWWINDOW);
}

RECT CAltTabManager::GetItemRectForIndex(int index) const
{
    const int logicalCols = m_columns;
    const int row = index / logicalCols;
    const int col = index % logicalCols;
    const int visibleRow = row - m_firstVisibleRow;

    int x = m_outerMargin + col * (m_itemWidth + m_gapX);
    int y = m_gridTop + visibleRow * (m_itemHeight + m_gapY);

    RECT rc{ x, y, x + m_itemWidth, y + m_itemHeight };
    return rc;
}

bool CAltTabManager::ShouldRespectCursorPosition(const POINT& pt) const
{
    return !PtInRegion(m_initialCursorRgn.handle(), pt.x, pt.y);
}

int CAltTabManager::HitTestItem(const POINT& pt) const
{
    const int visibleRows = GetVisibleRows();

    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
    {
        const int row = i / m_columns;
        if (row < m_firstVisibleRow || row >= m_firstVisibleRow + visibleRows)
            continue;

        const RECT rc = GetItemRectForIndex(i);
        if (PtInRect(&rc, pt))
            return i;
    }

    return -1;
}

bool CAltTabManager::IsActive() const
{
    return IsWindowVisible(m_hwndSwitcher);
}

bool CAltTabManager::IsSearchActive() const
{
    return GetFocus() == m_hwndSearch;
}

bool CAltTabManager::IsPointInSearchBox(const POINT& pt) const
{
    const RECT rc = GetSearchRect();
    return PtInRect(&rc, pt);
}

void CAltTabManager::PaintSwitcher(HWND hwnd)
{
    PAINTSTRUCT ps{};
    HDC hdcOrigin = BeginPaint(hwnd, &ps);

    RECT client{};
    GetClientRect(hwnd, &client);

    HDC hdc = {};
    HPAINTBUFFER hPaintBuffer = BeginBufferedPaint(hdcOrigin, &client, BPBF_COMPATIBLEBITMAP, NULL, &hdc);

    RECT contentRc = GetContentAreaRect();
    HGDIOBJ oldBorderPen = SelectObject(hdc, m_penBorder.handle());
    HGDIOBJ oldBrush = SelectObject(hdc, m_brushBackground.handle());
    Rectangle(hdc, client.left, client.top, client.right, client.bottom);
    SelectObject(hdc, oldBorderPen);
    SelectObject(hdc, oldBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_textColor.value());

    int visibleRows = GetVisibleRows();

    HRGN clipRgn = CreateRectRgn(
        contentRc.left,
        m_gridTop,
        contentRc.right,
        client.bottom - (m_selectedTitleHeight + m_bottomPadding + m_gridMarginY * 2)
    );
    SelectClipRgn(hdc, clipRgn);

    for (int i = 0; i < static_cast<int>(m_items.size()); ++i)
    {
        const int row = i / m_columns;
        if (row < m_firstVisibleRow || row >= m_firstVisibleRow + visibleRows)
            continue;

        const RECT rc = GetItemRectForIndex(i);
        const bool selected = (i == m_selectedIndex);
        const int iconX = rc.left + (m_itemWidth - m_iconSize) / 2;
        const int iconY = rc.top + (m_itemHeight - m_iconSize) / 2;
        if (selected)
        {
            HGDIOBJ oldBrush2 = SelectObject(hdc, m_brushSelectedItem.handle());
            HGDIOBJ oldPen = SelectObject(hdc, m_penSelectedItem.handle());
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
            SelectObject(hdc, oldBrush2);
            SelectObject(hdc, oldPen);
        }

        if (m_items[i].icon)
        {
            DrawIconEx(hdc, iconX, iconY, m_items[i].icon, m_iconSize, m_iconSize, 0, nullptr, DI_NORMAL);
        }
        else
        {
            RECT fallback = { iconX, iconY, iconX + m_iconSize, iconY + m_iconSize };
            FillRect(hdc, &fallback, m_brushItemNoIcon.handle());
        }
    }

    SelectClipRgn(hdc, nullptr);
    DeleteObject(clipRgn);

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_items.size()))
    {
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(hdc, m_font.handle()));

        RECT selectedTextRc = {
            contentRc.left,
            client.bottom - (m_selectedTitleHeight + m_bottomPadding),
            contentRc.right,
            client.bottom - m_bottomPadding
        };

        std::wstring title = m_items[m_selectedIndex].title;
        if (title.empty())
        {
            title = Utils::GetWindowText(m_items[m_selectedIndex].hwnd);
            m_items[m_selectedIndex].title = title;
        }

        DrawTextW(
            hdc,
            title.c_str(),
            -1,
            &selectedTextRc,
            DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS
        );

        SelectObject(hdc, oldFont);
    }

    EndBufferedPaint(hPaintBuffer, TRUE);
    EndPaint(hwnd, &ps);
}

void CAltTabManager::ScrollRows(int delta)
{
    const int totalRows = GetTotalRows();
    const int visibleRows = GetVisibleRows();
    const int maxFirstRow = totalRows - visibleRows;

    m_firstVisibleRow = min(maxFirstRow, max(0, m_firstVisibleRow + delta));

    UpdateScrollBar();
    InvalidateRect(m_hwndSwitcher, nullptr, TRUE);
}

LRESULT CAltTabManager::OnKeyboard(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        const bool active = IsActive();
        const bool searchActive = IsSearchActive();
        const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        const bool keyDown = (k->flags & LLKHF_UP) == 0;
        const bool keyUp = (k->flags & LLKHF_UP) != 0;

        if (k->vkCode == VK_LMENU || k->vkCode == VK_RMENU || k->vkCode == VK_MENU)
        {
            if (keyUp)
            {
                m_altDown = false;
                if (active && m_altTabSession && !searchActive)
                {
                    m_altTabSession = false;
                    ActivateCurrentSelection(CloseReason::ActivateByAltRelease);
                    return 1;
                }
                else if (m_altTabSession)
                {
                    m_altTabSession = false;
                    Utils::ForceAltKeyUp();
                    return 1;
                }
            }
            else
                m_altDown = true;
        }
        else if (k->vkCode == VK_TAB && keyDown && (m_altDown || m_altTabSession))
        {
            m_altTabSession = true;

            if (!active)
                PostMessageW(m_hwndSwitcher, WM_APP_SHOW_SWITCHER, 0, 0);
            else if (!m_items.empty())
                AdvanceSelection(Utils::IsShiftDown() ? VK_LEFT : VK_RIGHT);
            else
                return 0;

            return 1;
        }
        else if (((k->vkCode == VK_LEFT) || (k->vkCode == VK_RIGHT) || (k->vkCode == VK_UP) || (k->vkCode == VK_DOWN) || (k->vkCode == VK_HOME) || (k->vkCode == VK_END))
            && keyDown && !searchActive && m_altTabSession)
        {
            if (!m_items.empty())
            {
                AdvanceSelection(k->vkCode);
                return 1;
            }
            else
                return 0;
        }
        else if (active && !searchActive && keyDown && k->vkCode == VK_OEM_3)
        {
            m_altTabSession = false;
            m_altDownHiddenForSearch = m_altDown;
            if (m_altDownHiddenForSearch)
                Utils::ForceAltKeyUp();
            PostMessageW(m_hwndSwitcher, WM_APP_FOCUS_SEARCH, 0, 0);
            return 1;
        }
        else if (active && keyDown && k->vkCode == VK_ESCAPE)
        {
            const auto altDown = m_altDownHiddenForSearch | m_altDown;
            if (altDown)
                Utils::ForceAltKeyDown();
            HideSwitcher(!altDown, CloseReason::Escape);
            return 1;
        }
        else if (active && keyDown && !searchActive && k->vkCode == VK_PRIOR)
        {
            ScrollRows(-GetVisibleRows());
            return 1;
        }
        else if (active && keyDown && !searchActive && k->vkCode == VK_NEXT)
        {
            ScrollRows(GetVisibleRows());
            return 1;
        }
        else if (active && keyDown && k->vkCode == VK_RETURN)
        {
            if (m_selectedIndex >= 0)
            {
                PostMessageW(m_hwndMain, WM_APP_ACTIVATE_SELECTION, 0, 0);
                return 1;
            }
        }
    }

    return CallNextHookEx(m_keyboardHook, code, wParam, lParam);
}

LRESULT CAltTabManager::SearchEditWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_ACC_SELECTALL:
            SendMessageW(hwnd, EM_SETSEL, 0, -1);
            return 0;
        case ID_ACC_CTRLBACK:
            DeletePreviousWord();
            return 0;
        case ID_ACC_ESCAPE:
            HideSwitcher(m_altDown, CloseReason::Escape);
            return 0;
        case ID_ACC_RETURN:
            if (m_selectedIndex >= 0)
                PostMessageW(m_hwndMain, WM_APP_ACTIVATE_SELECTION, 0, 0);
            return 0;
        case ID_ACC_TAB:
        case ID_ACC_SHIFTTAB:
            AdvanceSelection((LOWORD(wParam) == ID_ACC_SHIFTTAB) ? VK_LEFT : VK_RIGHT);
            return 0;
        }
        break;
    }

    return CallWindowProcW(m_originalSearchProc, hwnd, msg, wParam, lParam);
}

LRESULT CAltTabManager::MainWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_APP_ACTIVATE_SELECTION:
        if (IsActive())
        {
            int selectedBeforeHide = m_selectedIndex;
            bool altWasHeld = m_altTabSession || m_altDown;

            const CloseReason reason = altWasHeld
                ? CloseReason::ActivateByEnterWhileAltHeld
                : CloseReason::ActivateByEnterWithoutAlt;
            ActivateCurrentSelection(reason);
            m_altTabSession = altWasHeld;
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        if (lParam == WM_LBUTTONDOWN)
        {
            OpenIniInNotepad();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        if (msg == WM_TASKBARCREATED)
        {
            CreateTrayIcon();
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CAltTabManager::SwitcherWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        BufferedPaintInit();
        break;

    case WM_DESTROY:
        BufferedPaintUnInit();
        break;

    case WM_CLOSE:
        return 0;

    case WM_WINDOWPOSCHANGED:
        if (((WINDOWPOS*)lParam)->flags & SWP_SHOWWINDOW)
        {
            POINT pt = {};
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            m_initialCursorRgn.attach(CreateRoundRectRgn(pt.x - m_minCursorOffsetForItemSelection,
                pt.y - m_minCursorOffsetForItemSelection,
                pt.x + m_minCursorOffsetForItemSelection,
                pt.y + m_minCursorOffsetForItemSelection,
                m_minCursorOffsetForItemSelection,
                m_minCursorOffsetForItemSelection));
        }
        break;

    case WM_APP_FOCUS_SEARCH:
        SetFocus(m_hwndSearch);
        return 0;

    case WM_APP_SHOW_SWITCHER:
        ShowSwitcher();
        return 0;

    case WM_CTLCOLOREDIT:
        if (reinterpret_cast<HWND>(lParam) == m_hwndSearch)
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, m_searchTextColor.value());
            SetBkColor(hdc, m_searchBackgroundColor.value());
            return reinterpret_cast<LRESULT>(m_brushSearchBackground.handle());
        }
        break;

    case WM_KILLFOCUS:
        if (reinterpret_cast<HWND>(wParam) != m_hwndSearch)
        {
            HideSwitcher(m_altDown, CloseReason::FocusLost);
            return 0;
        }
        break;

    case WM_ACTIVATEAPP:
        if (wParam == FALSE)
        {
            HideSwitcher(false, CloseReason::FocusLost);
            return 0;
        }
        break;

    case WM_SIZE:
        LayoutSearchControl();
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_SEARCH_EDIT && HIWORD(wParam) == EN_CHANGE && !m_internalSearchUpdate)
        {
            m_searchText = Utils::GetWindowText(m_hwndSearch);
            UpdateSearchAndRebuild();
            return 0;
        }
        break;

    case WM_VSCROLL:
    {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);

        int newPos = si.nPos;
        switch (LOWORD(wParam))
        {
        case SB_LINEUP:       newPos -= 1; break;
        case SB_LINEDOWN:     newPos += 1; break;
        case SB_PAGEUP:       newPos -= static_cast<int>(si.nPage); break;
        case SB_PAGEDOWN:     newPos += static_cast<int>(si.nPage); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:newPos = si.nTrackPos; break;
        case SB_TOP:          newPos = si.nMin; break;
        case SB_BOTTOM:       newPos = si.nMax; break;
        }

        newPos = min(si.nMax, max(0, newPos));

        if (newPos != m_firstVisibleRow)
        {
            m_firstVisibleRow = newPos;
            UpdateScrollBar();
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        ScrollRows(delta > 0 ? -1 : 1);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);

        const POINT pt = {
            static_cast<int>(static_cast<short>(LOWORD(lParam))),
            static_cast<int>(static_cast<short>(HIWORD(lParam)))
        };

        if (IsPointInSearchBox(pt))
        {
            SetFocus(m_hwndSearch);
            return 0;
        }

        int hit = HitTestItem(pt);
        if (hit >= 0)
        {
            m_selectedIndex = hit;
            PostMessageW(m_hwndMain, WM_APP_ACTIVATE_SELECTION, 0, 0);
            return 0;
        }

        if (m_hideOnMisclick)
            HideSwitcher(true, CloseReason::ClickOutside);

        return 0;
    }

    case WM_MOUSEMOVE:
    {
        const POINT pt = {
            static_cast<int>(static_cast<short>(LOWORD(lParam))),
            static_cast<int>(static_cast<short>(HIWORD(lParam)))
        };
        if (ShouldRespectCursorPosition(pt))
        {
            const int hit = HitTestItem(pt);
            if (hit >= 0 && hit != m_selectedIndex)
            {
                m_selectedIndex = hit;
                EnsureSelectionVisible();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (!IsSearchActive())
        {
            if (wParam == VK_RETURN && m_selectedIndex >= 0)
            {
                PostMessageW(m_hwndMain, WM_APP_ACTIVATE_SELECTION, 0, 0);
                return 0;
            }
        }
        break;

    case WM_PAINT:
        PaintSwitcher(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

template<>
void CSettingOption<int>::save(LPCWSTR iniPath) const {
	Utils::SetProfileInt(iniPath, m_section, m_iniName, value());
}

template<>
void CSettingOption<int>::load(LPCWSTR iniPath) {
	m_value = m_funcNormalize(Utils::GetProfileInt(iniPath, m_section, m_iniName, m_defaultValue));
}

template<>
void CSettingOption<std::wstring>::save(LPCWSTR iniPath) const {
	Utils::SetProfileString(iniPath, m_section, m_iniName, value().c_str());
}

template<>
void CSettingOption<std::wstring>::load(LPCWSTR iniPath) {
	m_value = m_funcNormalize(Utils::GetProfileString(iniPath, m_section, m_iniName, m_defaultValue));
}

template<>
void CSettingOption<COLORREF>::save(LPCWSTR iniPath) const {
	Utils::SetProfileInt(iniPath, m_section, m_iniName, value());
}

template<>
void CSettingOption<COLORREF>::load(LPCWSTR iniPath) {
	m_value = m_funcNormalize(Utils::GetProfileInt(iniPath, m_section, m_iniName, m_defaultValue));
}
