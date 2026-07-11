#include "CameraPositionChange.hpp"
#include "Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <Windows.h>

namespace Nemesis::CameraPositionChange
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::atomic<bool> g_enabled{ false };
        std::atomic<bool> g_running{ false };
        std::thread       g_worker;
        std::thread       g_hookThread;
        HHOOK             g_keyHook = nullptr;
        DWORD             g_hookThreadId = 0;

        std::uintptr_t CameraManager()
        {
            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            if (!base)
                return 0;
            return Mem::Read<std::uintptr_t>(base + Client::dwCameraManager);
        }

        std::uintptr_t EntFromHandle(std::uintptr_t base, std::uint32_t h)
        {
            if (h == 0xFFFFFFFF)
                return 0;
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list)
                return 0;
            const std::uint32_t i = h & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
        }

        std::uintptr_t ObservedPawn()
        {
            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            if (!base)
                return 0;
            const std::uintptr_t localPawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
            if (!localPawn)
                return 0;
            const std::uintptr_t obs = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pObserverServices);
            if (obs < 0x10000)
                return 0;
            const std::uint32_t h = Mem::Read<std::uint32_t>(obs + Schema::obs_hObserverTarget);
            const std::uintptr_t target = EntFromHandle(base, h);
            return (target && target != localPawn) ? target : 0;
        }

        void EnableThirdperson(std::uintptr_t mgr)
        {
            Mem::Write<float>(mgr + ThirdpersonCam::kDistance, CameraView::kDistance);
            Mem::Write<std::uint8_t>(mgr + ThirdpersonCam::kEnableFlag, 1);
        }

        void DisableThirdperson(std::uintptr_t mgr)
        {
            Mem::Write<std::uint8_t>(mgr + ThirdpersonCam::kEnableFlag, 0);
        }

        bool GameInForeground()
        {
            DWORD pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &pid);
            return pid == GetCurrentProcessId();
        }

        LRESULT CALLBACK KeyboardHook(int code, WPARAM wParam, LPARAM lParam)
        {
            if (code == HC_ACTION)
            {
                const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
                if (kb && kb->scanCode == CameraView::kToggleScan)
                {
                    static bool held = false;
                    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                    {
                        if (!held)
                        {
                            held = true;
                            if (GameInForeground())
                                Toggle();
                        }
                    }
                    else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                    {
                        held = false;
                    }
                }
            }
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }

        void HookThread()
        {
            g_hookThreadId = GetCurrentThreadId();
            g_keyHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, GetModuleHandleW(nullptr), 0);

            MSG msg;
            while (g_running.load() && GetMessageW(&msg, nullptr, 0, 0) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (g_keyHook)
            {
                UnhookWindowsHookEx(g_keyHook);
                g_keyHook = nullptr;
            }
        }

        void Worker()
        {
            bool lastEnabled = false;
            std::uintptr_t lastObserved = 0;

            while (g_running.load())
            {
                const std::uintptr_t mgr = CameraManager();
                const bool want = g_enabled.load();

                if (mgr)
                {
                    if (want && !lastEnabled)
                    {
                        EnableThirdperson(mgr);
                    }
                    else if (!want && lastEnabled)
                    {
                        DisableThirdperson(mgr);
                    }
                    else if (want)
                    {
                        if (Mem::Read<std::uint8_t>(mgr + ThirdpersonCam::kEnableFlag, 1) == 0)
                            Mem::Write<std::uint8_t>(mgr + ThirdpersonCam::kEnableFlag, 1);
                    }

                    lastEnabled = want;
                }

                if (want)
                {
                    const std::uintptr_t observed = ObservedPawn();
                    if (observed && observed != lastObserved)
                    {
                        lastObserved = observed;
                        NLOG("thirdperson spectating pawn %p", reinterpret_cast<void*>(observed));
                    }
                    else if (!observed)
                    {
                        lastObserved = 0;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true))
            return;

        g_worker = std::thread(Worker);
        g_hookThread = std::thread(HookThread);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        const std::uintptr_t mgr = CameraManager();
        if (mgr && g_enabled.load())
            DisableThirdperson(mgr);

        if (g_hookThreadId)
            PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);

        if (g_hookThread.joinable())
            g_hookThread.join();

        if (g_worker.joinable())
            g_worker.join();
    }

    void Toggle()
    {
        const bool state = !g_enabled.load();
        g_enabled.store(state);
        NLOG("thirdperson %s", state ? "ON" : "OFF");
    }

    bool IsEnabled()
    {
        return g_enabled.load();
    }
}
