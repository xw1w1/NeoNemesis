#include "SilentAim.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"
#include "../../../Server Packet Anomaly Guard/Search for Anomalies in Packages/PacketGuard.hpp"

#include "MinHook.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <Windows.h>

namespace Nemesis::SilentAim
{
    namespace Addr = Nemesis::Addresses;
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr float          kRad2Deg = 57.2957795f;
        constexpr std::uintptr_t kSeedFn  = 0xC81D80;   // sub_180C81D80: seed = SHA1(qAng, seedBase)

        using CreateMoveFn = void(__fastcall*)(void*, int, void*);
        using SeedFn       = int(__fastcall*)(void*, void*, int);
        CreateMoveFn g_orig     = nullptr;
        SeedFn       g_origSeed = nullptr;
        void*        g_seedTarget = nullptr;

        std::atomic<bool>  g_valid{ false };
        std::atomic<bool>  g_enabled{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        bool  g_loggedHook = false;
        DWORD g_lastSeedLog = 0;

        inline bool HeapPtr(std::uintptr_t p)
        {
            return p >= 0x10000 && p <= 0x7FFFFFFFFFFFull;
        }

        std::uintptr_t EntFromHandle(std::uintptr_t base, std::uint32_t h)
        {
            if (h == 0xFFFFFFFF) return 0;
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list) return 0;
            const std::uint32_t i = h & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk) return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
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

        std::uintptr_t FrameAngle(std::uintptr_t eng)
        {
            if (!eng) return 0;
            int cnt = Mem::Read<int>(eng + EngineInput::dwFrameCounter, 0);
            int idx = cnt % EngineInput::kRingSize; if (idx < 0) idx += EngineInput::kRingSize;
            const std::uintptr_t rec = eng + EngineInput::dwFrameRing + (std::uintptr_t)idx * EngineInput::kRecStride;
            const std::uintptr_t ptr = Mem::Read<std::uintptr_t>(rec + EngineInput::kRecPtr);
            return HeapPtr(ptr) ? ptr : 0;
        }

        // ����: ������� seedBase (r8d) � ����������-������
        int __fastcall hkSeed(void* rcx, void* rdx, int seedBase)
        {
            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            const DWORD now = GetTickCount();
            if (base && now - g_lastSeedLog > 250)
            {
                g_lastSeedLog = now;
                const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
                int shots = 0, entIdx = 0; float recoil = 0.f;
                if (HeapPtr(pawn))
                {
                    shots  = Mem::Read<int>(pawn + PawnCombat::m_iShotsFired, 0);
                    entIdx = Mem::Read<int>(pawn + Schema::m_iIDEntIndex, 0);
                    const std::uintptr_t ws = Mem::Read<std::uintptr_t>(pawn + Schema::m_pWeaponServices);
                    if (HeapPtr(ws))
                    {
                        const std::uintptr_t w = EntFromHandle(base, Mem::Read<std::uint32_t>(ws + Schema::m_hActiveWeapon));
                        if (HeapPtr(w)) recoil = Mem::Read<float>(w + Weapon::m_flRecoilIndex);
                    }
                }
                float qp = 0.f, qy = 0.f;
                if (HeapPtr((std::uintptr_t)rdx)) { qp = Mem::Read<float>((std::uintptr_t)rdx + 0); qy = Mem::Read<float>((std::uintptr_t)rdx + 4); }
                NLOG("[seed] seedBase=%d (0x%X) shots=%d ent=%d recoil=%.1f ang(%.2f,%.2f)",
                     seedBase, (unsigned)seedBase, shots, entIdx, recoil, qp, qy);
            }
            return g_origSeed(rcx, rdx, seedBase);
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
            const std::uintptr_t eng  = Mem::ModuleBase(Modules::kEngine);
            const std::uintptr_t self = reinterpret_cast<std::uintptr_t>(thisptr);

            if (!g_loggedHook && base)
            {
                g_loggedHook = true;
                NLOG("[silent] hook alive this=%p (F6)", thisptr);
            }

            bool injected = false;
            std::uintptr_t qa = 0;
            float op = 0.f, oy = 0.f, np = 0.f, ny = 0.f;

            const std::uintptr_t pawn = base ? Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn) : 0;
            const bool armed = base && eng && g_enabled.load() && g_valid.load()
                             && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                             && pawn && Mem::Read<int>(pawn + Schema::m_iHealth, 0) > 0;

            if (armed)
            {
                ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), np, ny);
                if (std::isfinite(np) && std::isfinite(ny))
                {
                    qa = FrameAngle(eng);
                    if (qa)
                    {
                        op = Mem::Read<float>(qa + 0);
                        oy = Mem::Read<float>(qa + 4);
                        Mem::Write<float>(qa + 0, np);
                        Mem::Write<float>(qa + 4, ny);
                        injected = true;
                    }
                }
            }

            g_orig(thisptr, a2, a3);

            if (injected)
            {
                Mem::Write<float>(qa + 0, op);
                Mem::Write<float>(qa + 4, oy);
                Nemesis::PacketGuard::SanitizeCommands(self, np, ny);
                Nemesis::PacketGuard::SanitizeBaseAngle(self, np, ny);
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

        void* target = reinterpret_cast<void*>(base + Addr::SilentAim::fnCreateMove);
        if (MH_CreateHook(target, &hkCreateMove, reinterpret_cast<void**>(&g_orig)) != MH_OK)
        { NERR("[silent] CreateHook failed"); return; }
        if (MH_EnableHook(target) != MH_OK)
        { NERR("[silent] EnableHook failed"); return; }

        g_seedTarget = reinterpret_cast<void*>(base + kSeedFn);
        if (MH_CreateHook(g_seedTarget, &hkSeed, reinterpret_cast<void**>(&g_origSeed)) == MH_OK)
            MH_EnableHook(g_seedTarget);
        else
        { NERR("[silent] seed CreateHook failed"); g_seedTarget = nullptr; }

        NLOG("[silent] hooked CreateMove + seed diag (F6)");
    }

    void Shutdown()
    {
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (base)
            MH_DisableHook(reinterpret_cast<void*>(base + Addr::SilentAim::fnCreateMove));
        if (g_seedTarget)
            MH_DisableHook(g_seedTarget);
    }
}
