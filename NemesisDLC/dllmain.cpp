#include <Windows.h>

#include "Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/CameraPositionChange.hpp"
#include "Miscellaneous Functions/UnusualNewVisions/CustomSkinsgun/CustomSkins.hpp"
#include "Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

static DWORD WINAPI Bootstrap(LPVOID)
{
    Nemesis::Logs::SetEnabled(true);
    NLOG("NemesisLoader injected, build %s", __DATE__);
    Nemesis::CameraPositionChange::Start();
    Nemesis::CustomSkins::Start();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, Bootstrap, hModule, 0, nullptr);
    }

    return TRUE;
}
