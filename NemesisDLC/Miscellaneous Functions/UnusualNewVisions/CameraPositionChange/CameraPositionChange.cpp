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

        std::uintptr_t CameraManager()
        {
            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            if (!base)
                return 0;
            return Mem::Read<std::uintptr_t>(base + Client::dwCameraManager);
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

        void HotkeyPoll()
        {
            static bool prev = false;
            const bool now = (GetAsyncKeyState(CameraView::kToggleKey) & 0x8000) != 0;
            if (now && !prev)
                Toggle();
            prev = now;
        }

        void Worker()
        {
            bool lastEnabled = false;

            while (g_running.load())
            {
                HotkeyPoll();

                const std::uintptr_t mgr = CameraManager();
                if (mgr)
                {
                    const bool want = g_enabled.load();

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

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true))
            return;

        g_worker = std::thread(Worker);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        const std::uintptr_t mgr = CameraManager();
        if (mgr && g_enabled.load())
            DisableThirdperson(mgr);

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
