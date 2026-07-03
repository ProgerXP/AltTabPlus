#include <string>
#include <WinDef.h>

static constexpr LPCWSTR INI_UI_SECTION = L"AltTabPlus";
static constexpr LPCWSTR INI_COLORS_SECTION = L"Colors";
static constexpr LPCWSTR INI_FEATURES_SECTION = L"Features";

namespace Utils
{

std::wstring GetWindowText(HWND hwnd);
bool IsWindowCloaked(HWND hwnd);
HWND GetAltTabRepresentative(HWND hwnd);
HICON GetSmallWindowIcon(HWND hwnd);
bool IsAltDown();
bool IsShiftDown();
bool IsReallyMinimized(HWND hwnd, LPRECT pRectRestored);
bool PtInRect(const RECT& rectTarget, const RECT& rectOrigin);
void ForceAltKeyUp();
void ForceAltKeyDown();
RECT GetActiveMonitorRect();

int GetProfileInt(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const int defaultValue);
bool GetProfileBool(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const bool defaultValue);
std::wstring GetProfileString(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const std::wstring& defaultValue);
bool SetProfileInt(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const int value);
bool SetProfileString(LPCWSTR ini, LPCWSTR section, LPCWSTR name, const std::wstring& value);

} // namespace Utils
