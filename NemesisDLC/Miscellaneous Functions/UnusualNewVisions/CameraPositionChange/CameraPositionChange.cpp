#include "CameraPositionChange.hpp"
#include "SigScan.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>
#include <Windows.h>
#include <MinHook.h>

namespace Nemesis::CameraPositionChange
{
    using namespace Nemesis::Addresses;

    namespace
    {
        struct Vec3
        {
            float x;
            float y;
            float z;
        };

        using ViewSetupFn = void(__fastcall*)(void* self, Vec3* origin, Vec3* angles);

        std::atomic<bool> g_enabled{ false };
        std::atomic<bool> g_running{ false };
        std::atomic<bool> g_hooked{ false };
        std::thread       g_worker;
        ViewSetupFn       g_original = nullptr;

        void AnglesToForward(const Vec3& angles, Vec3& forward)
        {
            const float deg2rad = 3.14159265358979323846f / 180.0f;
            const float p = angles.x * deg2rad;
            const float y = angles.y * deg2rad;
            const float cp = std::cos(p);
            const float sp = std::sin(p);
            const float cy = std::cos(y);
            const float sy = std::sin(y);
            forward.x = cp * cy;
            forward.y = cp * sy;
            forward.z = -sp;
        }

        void __fastcall HkViewSetup(void* self, Vec3* origin, Vec3* angles)
        {
            g_original(self, origin, angles);

            if (!g_enabled.load() || !origin || !angles)
                return;

            Vec3 forward{};
            AnglesToForward(*angles, forward);

            origin->x -= forward.x * CameraView::kDistance;
            origin->y -= forward.y * CameraView::kDistance;
            origin->z -= forward.z * CameraView::kDistance;
            origin->z += CameraView::kHeight;
        }

        bool InstallHook()
        {
            if (std::strlen(Sig::kViewSetup) == 0)
            {
                NWARN("view hook pending: Sig::kViewSetup is empty, hook not installed");
                return false;
            }

            const std::uintptr_t target = Sig::Find(Modules::kClient, Sig::kViewSetup);
            if (!target)
            {
                NERR("view function not found by signature");
                return false;
            }

            if (MH_Initialize() != MH_OK && MH_Initialize() != MH_ERROR_ALREADY_INITIALIZED)
            {
                NERR("MH_Initialize failed");
                return false;
            }

            if (MH_CreateHook(reinterpret_cast<void*>(target), &HkViewSetup,
                              reinterpret_cast<void**>(&g_original)) != MH_OK)
            {
                NERR("MH_CreateHook failed at 0x%llX", static_cast<unsigned long long>(target));
                return false;
            }

            if (MH_EnableHook(reinterpret_cast<void*>(target)) != MH_OK)
            {
                NERR("MH_EnableHook failed");
                return false;
            }

            NLOG("view hook installed at 0x%llX", static_cast<unsigned long long>(target));
            return true;
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
            g_hooked.store(InstallHook());

            while (g_running.load())
            {
                HotkeyPoll();
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

        if (g_worker.joinable())
            g_worker.join();
    }

    void Toggle()
    {
        if (!g_hooked.load())
        {
            NWARN("toggle ignored: view hook not active");
            return;
        }

        const bool state = !g_enabled.load();
        g_enabled.store(state);
        NLOG("CameraPositionChange toggled -> %s", state ? "ON" : "OFF");
    }

    bool IsEnabled()
    {
        return g_enabled.load();
    }
}
