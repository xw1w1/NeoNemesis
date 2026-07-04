#include "SilentAim.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include "MinHook.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <Windows.h>

namespace Nemesis::SilentAim
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr float kRad2Deg = 57.2957795f;
        constexpr std::uintptr_t kSerGlobal = 0x685910;
        constexpr std::size_t     kSerVIndex = 0xB8 / 8;

        using CreateMoveFn = void(__fastcall*)(void*, int, void*);
        using SerFn        = void*(__fastcall*)(void*, int, void*, int);
        CreateMoveFn g_orig = nullptr;
        SerFn        g_origSer = nullptr;
        void*        g_serTarget = nullptr;

        std::atomic<bool>  g_valid{ false };
        std::atomic<bool>  g_enabled{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        bool g_loggedHook = false;
        bool g_loggedSer  = false;

        inline bool HeapPtr(std::uintptr_t p)
        {
            return p >= 0x10000 && p <= 0x7FFFFFFFFFFFull;
        }

        void ComputeAngle(std::uintptr_t pawn, float hx, float hy, float hz,
                          float& pitch, float& yaw)
        {
            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!HeapPtr(scene)) { pitch = 0.f; yaw = 0.f; return; }
            const float ex = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
            const float ey = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
            const float ez = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8)
                           + Mem::Read<float>(pawn + Schema::m_vecViewOffset + 8);

            const float dx = hx - ex, dy = hy - ey, dz = hz - ez;
            const float d2 = std::sqrt(dx * dx + dy * dy);
            yaw   = std::atan2(dy, dx) * kRad2Deg;
            pitch = -std::atan2(dz, d2) * kRad2Deg;
        }

        bool CurrentAim(std::uintptr_t base, float& p, float& y)
        {
            if (!g_enabled.load() || !g_valid.load()) return false;
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return false;
            const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
            if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0) return false;
            ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), p, y);
            return std::isfinite(p) && std::isfinite(y);
        }

        std::uintptr_t LastSlot(std::uintptr_t self)
        {
            if (!HeapPtr(self)) return 0;
            const int count = Mem::Read<int>(self + Addresses::SilentAim::kCmdCount, 0);
            if (count <= 0 || count > 64) return 0;
            const std::uintptr_t data = Mem::Read<std::uintptr_t>(self + Addresses::SilentAim::kCmdData);
            if (!HeapPtr(data)) return 0;
            const std::uintptr_t slot = data + (std::uintptr_t)(count - 1) * Addresses::SilentAim::kCmdStride;
            return HeapPtr(slot) ? slot : 0;
        }

        void* __fastcall hkSer(void* a, int b, void* c, int d)
        {
            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            if (!g_loggedSer && base) { g_loggedSer = true; NLOG("[silent] ser hook alive"); }

            if (!base) return g_origSer(a, b, c, d);

            const std::uintptr_t va = base + Client::dwViewAngles;
            float p, y;
            if (!CurrentAim(base, p, y))
                return g_origSer(a, b, c, d);

            const float op = Mem::Read<float>(va + 0);
            const float oy = Mem::Read<float>(va + 4);
            Mem::Write<float>(va + 0, p);
            Mem::Write<float>(va + 4, y);
            void* r = g_origSer(a, b, c, d);
            Mem::Write<float>(va + 0, op);
            Mem::Write<float>(va + 4, oy);
            return r;
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

            if (!base || !g_enabled.load() || !g_valid.load()) return;
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

            const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
            if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0) return;

            const std::uintptr_t slot = LastSlot(self);
            if (!slot) return;

            float np = 0.f, ny = 0.f;
            ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), np, ny);
            if (!std::isfinite(np) || !std::isfinite(ny)) return;

            Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleA + 0, np);
            Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleA + 4, ny);
            Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleB + 0, np);
            Mem::Write<float>(slot + Addresses::SilentAim::kCmdAngleB + 4, ny);
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

        const std::uintptr_t eng = Mem::ModuleBase(Modules::kEngine);
        if (eng)
        {
            const std::uintptr_t obj = Mem::Read<std::uintptr_t>(eng + kSerGlobal);
            if (HeapPtr(obj))
            {
                const std::uintptr_t vt = Mem::Read<std::uintptr_t>(obj);
                if (HeapPtr(vt))
                {
                    g_serTarget = reinterpret_cast<void*>(Mem::Read<std::uintptr_t>(vt + kSerVIndex * 8));
                    if (HeapPtr(reinterpret_cast<std::uintptr_t>(g_serTarget)))
                    {
                        if (MH_CreateHook(g_serTarget, &hkSer, reinterpret_cast<void**>(&g_origSer)) == MH_OK)
                            MH_EnableHook(g_serTarget);
                        else
                            NERR("[silent] ser CreateHook failed");
                    }
                }
            }
        }
        NLOG("[silent] hooked CreateMove @ client+0x%llX", (unsigned long long)Addresses::SilentAim::fnCreateMove);
    }

    void Shutdown()
    {
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (base)
            MH_DisableHook(reinterpret_cast<void*>(base + Addresses::SilentAim::fnCreateMove));
        if (g_serTarget)
            MH_DisableHook(g_serTarget);
    }
}
