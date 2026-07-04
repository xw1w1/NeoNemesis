#include "SilentAim.hpp"
#include "AimMath.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include "MinHook.h"

#include <atomic>
#include <cmath>
#include <Windows.h>

namespace Nemesis::SilentAim
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr float kRad2Deg = 57.2957795f;
        constexpr std::ptrdiff_t kCmdAngleNet = 0x258;  // CBaseUserCmdPB.viewangles (в сеть)
        using CreateMoveFn = void(__fastcall*)(void*, int, void*);
        CreateMoveFn g_orig = nullptr;

        std::atomic<bool>  g_valid{ false };
        std::atomic<bool>  g_enabled{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        bool g_loggedHook = false;
        bool g_loggedNet = false;

        inline bool HeapPtr(std::uintptr_t p)
        {
            return p >= 0x10000 && p <= 0x7FFFFFFFFFFFull;
        }

        void ComputeAngle(std::uintptr_t pawn, float hx, float hy, float hz,
                          float& pitch, float& yaw)
        {
            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            const float ex = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
            const float ey = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
            const float ez = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8)
                           + Mem::Read<float>(pawn + Schema::m_vecViewOffset + 8);

            const float dx = hx - ex, dy = hy - ey, dz = hz - ez;
            const float d2 = std::sqrt(dx * dx + dy * dy);
            yaw   = std::atan2(dy, dx) * kRad2Deg;
            pitch = -std::atan2(dz, d2) * kRad2Deg;
        }

        void __fastcall hkCreateMove(void* thisptr, int a2, void* a3)
        {
            static bool prev = false;
            const bool curKey = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
            if (curKey && !prev)
            {
                const bool now = !g_enabled.load();
                g_enabled.store(now);
                NLOG("[silent] %s", now ? "ON" : "OFF");
            }
            prev = curKey;

            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            const std::uintptr_t self = reinterpret_cast<std::uintptr_t>(thisptr);

            if (!g_loggedHook && base)
            {
                g_loggedHook = true;
                NLOG("[silent] hook alive this=%p (F6)", thisptr);
            }

            g_orig(thisptr, a2, a3);

            if (!base || !g_enabled.load() || !g_valid.load())
                return;
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                return;

            const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
            if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
                return;

            const int count = Mem::Read<int>(self + Addresses::SilentAim::kCmdCount, 0);
            const std::uintptr_t data = Mem::Read<std::uintptr_t>(self + Addresses::SilentAim::kCmdData);
            if (count <= 0 || !HeapPtr(data))
                return;

            float np = 0.f, ny = 0.f;
            ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), np, ny);

            float punchP = 0.f, punchY = 0.f;
            const std::uintptr_t punchSvc = Mem::Read<std::uintptr_t>(pawn + PawnCombat::m_pAimPunchServices);
            if (HeapPtr(punchSvc))
            {
                punchP = Mem::Read<float>(punchSvc + PawnCombat::ap_predictableBaseAngle + 0);
                punchY = Mem::Read<float>(punchSvc + PawnCombat::ap_predictableBaseAngle + 4);
            }
            const float wp = AimMath::NormalizeAngle(np - (std::isfinite(punchP) ? punchP : 0.f) * AimFn::kRecoilScale);
            const float wy = AimMath::NormalizeAngle(ny - (std::isfinite(punchY) ? punchY : 0.f) * AimFn::kRecoilScale);

            const int n = (count > 64) ? 64 : count;
            for (int i = 0; i < n; ++i)
            {
                const std::uintptr_t slot = data + (std::uintptr_t)i * Addresses::SilentAim::kCmdStride;
                Mem::Write<float>(slot + kCmdAngleNet + 0, wp);   // сетевой viewangles
                Mem::Write<float>(slot + kCmdAngleNet + 4, wy);
                Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleA + 0, wp);
                Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleA + 4, wy);
                Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleB + 0, wp);
                Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleB + 4, wy);
            }

            if (!g_loggedNet)
            {
                g_loggedNet = true;
                const std::uintptr_t last = data + (std::uintptr_t)(count - 1) * Addresses::SilentAim::kCmdStride;
                NLOG("[silent] wrote net+0x258 count=%d net(p=%.2f y=%.2f) A(p=%.2f y=%.2f)",
                     count,
                     Mem::Read<float>(last + kCmdAngleNet + 0), Mem::Read<float>(last + kCmdAngleNet + 4),
                     Mem::Read<float>(last + Addresses::SilentAim::kCmdAngleA + 0),
                     Mem::Read<float>(last + Addresses::SilentAim::kCmdAngleA + 4));
            }
        }
    }

    void SetTarget(float x, float y, float z)
    {
        g_hx.store(x); g_hy.store(y); g_hz.store(z);
        g_valid.store(true);
    }

    void ClearTarget()
    {
        g_valid.store(false);
    }

    void Init()
    {
        MH_Initialize();
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base) { NERR("[silent] client.dll not found"); return; }

        void* target = reinterpret_cast<void*>(base + Addresses::SilentAim::fnCreateMove);
        if (MH_CreateHook(target, &hkCreateMove, reinterpret_cast<void**>(&g_orig)) != MH_OK)
        { NERR("[silent] CreateHook failed"); return; }
        if (MH_EnableHook(target) != MH_OK)
        { NERR("[silent] EnableHook failed"); return; }
        NLOG("[silent] hooked CreateMove @ client+0x%llX (F6)",
             (unsigned long long)Addresses::SilentAim::fnCreateMove);
    }

    void Shutdown()
    {
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (base)
            MH_DisableHook(reinterpret_cast<void*>(base + Addresses::SilentAim::fnCreateMove));
    }
}
