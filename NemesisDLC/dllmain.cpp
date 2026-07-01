#include <Windows.h>

#include <asmjit/asmjit.h>

#include "Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/CameraPositionChange.hpp"
#include "Miscellaneous Functions/UnusualNewVisions/CustomSkinsgun/CustomSkins.hpp"
#include "Miscellaneous Functions/UnusualNewVisions/InventoryGive/InventoryGive.hpp"
#include "Miscellaneous Utilities/sc_cheat/SvCheats.hpp"
#include "RenderDrx11/RenderHook.hpp"
#include "RenderDrx11/UsedHook.hpp"
#include "Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

static void AsmjitSelfTest()
{
    asmjit::JitRuntime rt;
    asmjit::CodeHolder code;
    code.init(rt.environment());

    asmjit::x86::Assembler a(&code);
    a.mov(asmjit::x86::eax, 0x1337);
    a.ret();

    int (*fn)() = nullptr;
    if (rt.add(&fn, &code) == asmjit::kErrorOk && fn)
    {
        NLOG("asmjit self-test returned 0x%X", fn());
        rt.release(fn);
    }
}

static DWORD WINAPI Bootstrap(LPVOID)
{
    Nemesis::Logs::SetEnabled(true);
    NLOG("NemesisLoader injected, build %s", __DATE__);

    AsmjitSelfTest();

    Nemesis::SvCheats::Start();
    Nemesis::CameraPositionChange::Start();
    // Nemesis::CustomSkins::Start();   // система скинов отключена по запросу
    // Nemesis::InventoryGive::Start();   // путь инвентаря-меню отложен (тупик для ножа в руке)
    Nemesis::RenderHook::Start();
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
