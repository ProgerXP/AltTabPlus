#include "stdafx.h"
#include "Utils.h"
#include <functional>
#include <map>

struct AltTabItem
{
    HWND hwnd = nullptr;
    std::wstring title;
    HICON icon = nullptr;
};

enum class IsAltTabCandidateResult
{
    Success = 0,
    NotWindowOrNotVisible,
    SkipMinimized,
    SkipChild,
    SkipTool,
    SkipCloaked,
    SkipOwned,
    SkipEmptyTitle,
    SkipFailedGetWindowRect,
    SkipEmptyRect,
    SkipNotInRect
};

enum class CloseReason
{
    None,
    ActivateByAltRelease,
    ActivateByEnterWhileAltHeld,
    ActivateByEnterWithoutAlt,
    SearchMode,
    Escape,
    ClickOutside,
    FocusLost,
    Other
};

template <class T>
class CSettingOption
{
private:
    T m_value = 0;
    const T m_defaultValue = 0;
    std::function<T(const T&)> m_funcNormalize;
    LPCWSTR m_iniName = 0;
    LPCWSTR m_section = 0;
public:
    explicit CSettingOption(LPCWSTR iniName, LPCWSTR section, const T& defaultValue,
        const std::function<T(const T&)> funcNormalize = [](const T& v) { return v; })
        : m_iniName(iniName), m_section(section), m_defaultValue(defaultValue), m_value(defaultValue), m_funcNormalize(funcNormalize) {};
    void save(LPCWSTR iniPath) const;
    void load(LPCWSTR iniPath);
    operator T() const { return m_value; }
    T value() const { return m_value; }
};

typedef CSettingOption<int> CIntSettingOption;
typedef CSettingOption<std::wstring> CStringSettingOption;
typedef CSettingOption<COLORREF> CRGBSettingOption;

template <class T>
class CGDIObject
{
protected:
    T m_handle = NULL;
public:
    explicit CGDIObject(T h = {}) : m_handle(h) {}
    void attach(T h) {
        if (m_handle)
            DeleteObject(m_handle);
        m_handle = h;
    }
    virtual ~CGDIObject() {
        if (m_handle) {
            DeleteObject(m_handle);
            m_handle = NULL;
        }
    }
    T handle() const {
        return m_handle;
    }
};

typedef CGDIObject<HFONT> CFont;
typedef CGDIObject<HBRUSH> CBrush;
typedef CGDIObject<HPEN> CPen;
typedef CGDIObject<HRGN> CRgn;

class CAltTabManager
{
public:
    CAltTabManager(HINSTANCE hInstance);
    ~CAltTabManager();

    int Run();

private:
    static constexpr const wchar_t* MAIN_CLASS_NAME = L"AltTabPlusMainWindowClass";
    static constexpr const wchar_t* SWITCHER_CLASS_NAME = L"AltTabPlusSwitcherWindowClass";
    static constexpr const wchar_t* INI_HEADER = L"; https://github.com/ProgerXP/AltTabPlus";

    std::map<HWND, IsAltTabCandidateResult> m_mapEnumFailures;

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwndMain = nullptr;
    HWND m_hwndSwitcher = nullptr;
    HWND m_hwndSearch = nullptr;
    HHOOK m_keyboardHook = nullptr;
    NOTIFYICONDATAW m_nid{};

    HANDLE m_hSingleInstanceMutex = nullptr;

    CPen m_penBorder;
    CBrush m_brushBackground;
    CBrush m_brushSelectedItem;
    CPen m_penSelectedItem;
    CBrush m_brushItemNoIcon;
    CFont m_font;
    CBrush m_brushSearchBackground;
    CFont m_fontSearch;

    WNDPROC m_originalSearchProc = nullptr;

    bool m_altDown = false;
    bool m_altDownHiddenForSearch = false;
    bool m_altTabSession = false;
    bool m_internalSearchUpdate = false;
    bool m_hideSwitcherActive = false;

    std::wstring m_searchText;
    std::vector<HWND> m_mru;
    std::vector<AltTabItem> m_items;

    int m_selectedIndex = -1;
    int m_firstVisibleRow = 0;
    CIntSettingOption m_columns;
    CIntSettingOption m_ignoreMinimized;
    CIntSettingOption m_autoResizeOnFilterChange;
    CIntSettingOption m_iconSize;
    CIntSettingOption m_itemWidth;
    CIntSettingOption m_itemHeight;
    CIntSettingOption m_gapX;
    CIntSettingOption m_gapY;
    CIntSettingOption m_gridTop;
    CIntSettingOption m_gridMarginY;
    CIntSettingOption m_outerMargin;

    CRGBSettingOption m_borderColor;
    CIntSettingOption m_borderSize;
    CRGBSettingOption m_textColor;
    CRGBSettingOption m_backgroundColor;
    CRGBSettingOption m_selectedItemBackgroundColor;
    CRGBSettingOption m_selectedItemBorderColor;
    CIntSettingOption m_selectedItemBorderWidth;
    CRGBSettingOption m_itemColorNoIcon;
    CIntSettingOption m_fontSize;
    CStringSettingOption m_fontFaceName;

    CIntSettingOption m_searchTop;
    CIntSettingOption m_searchHeight;
    CIntSettingOption m_searchMarginX;
    CIntSettingOption m_searchMarginY;
    CIntSettingOption m_searchFontSize;
    CStringSettingOption m_searchFontFaceName;
    CRGBSettingOption m_searchTextColor;
    CRGBSettingOption m_searchBackgroundColor;

    CIntSettingOption m_bottomPadding;
    CIntSettingOption m_selectedTitleHeight;
    CIntSettingOption m_maxMenuHeight;
    CIntSettingOption m_minMenuWidth;
    CIntSettingOption m_minMenuHeight;

    CIntSettingOption m_minCursorOffsetForItemSelection;

    CIntSettingOption m_showActiveMonitorWindowsOnly;
    CIntSettingOption m_showTrayIcon;
    CIntSettingOption m_hideOnMisclick;
    CIntSettingOption m_windowActivationMethod;

    int m_fixedSwitcherWidth = 0;
    CRgn m_initialCursorRgn;
    RECT m_rectMonitorForWindowsEnumeration = {};

    std::wstring m_iniPath;

    static CAltTabManager* s_instance;

private:
    void InitializeIniPath();
    void EnsureIniExists() const;
    void OpenIniInNotepad();
    void LoadSettings();
    bool AcquireSingleInstance();
    bool ShouldForceAltUp(CloseReason reason) const;
    int GetTotalRows() const;
    int GetVisibleRowsForHeight(int height) const;
    int GetVisibleRows() const;
    int GetVisibleScrollBarWidth() const;
    RECT GetContentAreaRect() const;
    RECT GetSearchRect() const;
    static BOOL CALLBACK EnumWindowsFunc(HWND hwnd, LPARAM lParam);
    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SwitcherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SearchEditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool RegisterClasses();
    bool CreateSearchControl();
    void LayoutSearchControl();
    bool CreateWindows();
    bool CreateTrayIcon();
    void RemoveTrayIcon();
    bool InstallHooks();
    void Cleanup();
    void FreeItemIcons();
    IsAltTabCandidateResult IsAltTabCandidate(HWND hwnd, const RECT& rectMonitor) const;
    BOOL OnEnumWindow(HWND hwnd);
    void BuildWindowList(const bool reload);
    SIZE CalculateSwitcherSize() const;
    void UpdateScrollBar();
    void EnsureSelectionVisible();
    void ActivateCurrentSelection(const CloseReason& reason);
    void DeletePreviousWord();
    void ShowSwitcher();
    void HideSwitcher(const bool manageAltKey, const CloseReason reason);
    void AdvanceSelection(const DWORD vkCode);
    void UpdateSearchAndRebuild();
    void UpdateWindowPos() const;
    RECT GetItemRectForIndex(int index) const;
    bool ShouldRespectCursorPosition(const POINT& pt) const;
    int HitTestItem(const POINT& pt) const;
    bool IsActive() const;
    bool IsSearchActive() const;
    bool IsPointInSearchBox(const POINT& pt) const;
    void PaintSwitcher(HWND hwnd);
    void ScrollRows(int delta);
    LRESULT OnKeyboard(int code, WPARAM wParam, LPARAM lParam);
    LRESULT SearchEditWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT MainWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT SwitcherWndProcImpl(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
