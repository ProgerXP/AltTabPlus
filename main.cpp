#include "stdafx.h"
#include "AltTabManager.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif

CAltTabManager* CAltTabManager::s_instance = nullptr;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    CAltTabManager manager(hInstance);
    return manager.Run();
}