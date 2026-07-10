#include "SilentAim.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"
#include "../../../Server Packet Anomaly Guard/Search for Anomalies in Packages/PacketGuard.hpp"
#include "../JumpSystem/JumpSystem.hpp"
#include "../../../CUSTOM RENDER/CustomModel/CustomModel.hpp"
#include "AimMath.hpp"

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

        using CreateMoveFn = void(__fastcall*)(void*, int, void*);
        using SeedFn       = int(__fastcall*)(void*, void*, int);
        CreateMoveFn g_orig     = nullptr;
        SeedFn       g_origSeed = nullptr;
        void*        g_seedTarget = nullptr;

        std::atomic<bool>  g_valid{ false };
        std::atomic<bool>  g_enabled{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        std::atomic<std::uint32_t> g_seedBase{ 0 };
        std::atomic<bool>  g_haveSeed{ false };
        std::atomic<int>   g_tickOffset{ 0 };   // seedBase - clientTick (глубина предсказания, live из hkSeed)
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

        // seedBase выстрела == engine clientTickCount (подтверждено: diff=0). Читаем заранее в CreateMove.
        int ClientTick(std::uintptr_t eng)
        {
            if (!eng) return -1;
            const std::uintptr_t nc = Mem::Read<std::uintptr_t>(eng + EngineNet::dwNetworkGameClient);
            if (!HeapPtr(nc)) return -1;
            return Mem::Read<int>(nc + EngineNet::clientTickCount, -1);
        }

        // Предиктор: компенсация разброса/inaccuracy/recoil. ТОЛЬКО чтение полей (без vtable — vtable крашил).
        // Валидирует значения; при мусоре откатывает на чистый угол (return false).
        bool SolveAimAngle(std::uintptr_t base, std::uintptr_t pawn, std::uint32_t seedBase,
                           float aimP, float aimY, float& outP, float& outY)
        {
            __try
            {
                const std::uintptr_t ws = Mem::Read<std::uintptr_t>(pawn + Schema::m_pWeaponServices);
                if (!HeapPtr(ws)) return false;
                const std::uintptr_t weapon =
                    EntFromHandle(base, Mem::Read<std::uint32_t>(ws + Schema::m_hActiveWeapon));
                if (!HeapPtr(weapon)) return false;
                const std::uintptr_t vdata = Mem::Read<std::uintptr_t>(weapon + Weapon::m_pVData);
                if (!HeapPtr(vdata)) return false;

                float inacc  = Mem::Read<float>(weapon + Weapon::m_fAccuracyPenalty, 0.f);
                float spread = Mem::Read<float>(vdata + Weapon::vd_flSpread, 0.f);
                if (!std::isfinite(inacc)  || inacc  < 0.f || inacc  > AimFn::kMaxInacc)  inacc  = 0.f;
                if (!std::isfinite(spread) || spread < 0.f || spread > AimFn::kMaxSpread) spread = 0.f;

                float punchP = 0.f, punchY = 0.f;
                const std::uintptr_t aps = Mem::Read<std::uintptr_t>(pawn + PawnCombat::m_pAimPunchServices);
                if (HeapPtr(aps))
                {
                    punchP = Mem::Read<float>(aps + PawnCombat::ap_predictableBaseAngle + 0, 0.f);
                    punchY = Mem::Read<float>(aps + PawnCombat::ap_predictableBaseAngle + 4, 0.f);
                    if (!std::isfinite(punchP)) punchP = 0.f;
                    if (!std::isfinite(punchY)) punchY = 0.f;
                }

                AimMath::SolveWriteAngle(aimP, aimY, punchP, punchY, seedBase,
                                        inacc, spread, AimFn::kRecoilScale, outP, outY);
                return std::isfinite(outP) && std::isfinite(outY);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
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
            g_seedBase.store(static_cast<std::uint32_t>(seedBase));
            g_haveSeed.store(true);

            const std::uintptr_t engB = Mem::ModuleBase(Modules::kEngine);
            const int liveTick = ClientTick(engB);
            if (liveTick >= 0)
                g_tickOffset.store(seedBase - liveTick);   // глубина предсказания (адаптив к пингу)

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
                int tick = -1;
                const std::uintptr_t eng = Mem::ModuleBase(Modules::kEngine);
                if (eng)
                {
                    const std::uintptr_t nc = Mem::Read<std::uintptr_t>(eng + EngineNet::dwNetworkGameClient);
                    if (HeapPtr(nc)) tick = Mem::Read<int>(nc + EngineNet::clientTickCount, -1);
                }
                NLOG("[seed] seedBase=%d tick=%d diff=%d shots=%d recoil=%.1f ang(%.2f,%.2f)",
                     seedBase, tick, (tick >= 0 ? seedBase - tick : 0), shots, recoil, qp, qy);
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

            Nemesis::JumpSystem::JumpTick(base);
            Nemesis::CustomModel::Render();

            if (!g_loggedHook && base)
            {
                g_loggedHook = true;
                NLOG("[silent] hook alive this=%p (F6)", thisptr);
            }

            bool haveShot = false;      // цель есть и угол посчитан — база silent
            bool ringInjected = false;  // pre-inject в engine-ring удался (необязателен)
            std::uintptr_t qa = 0;
            float op = 0.f, oy = 0.f, np = 0.f, ny = 0.f;

            const std::uintptr_t pawn = base ? Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn) : 0;
            const bool alive = pawn && Mem::Read<int>(pawn + Schema::m_iHealth, 0) > 0;
            const bool lmb   = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool armed = base && g_enabled.load() && g_valid.load() && lmb && alive;

            {
                static DWORD s_dbg = 0;
                if (base && g_enabled.load() && GetTickCount() - s_dbg > 400)
                {
                    s_dbg = GetTickCount();
                    const int count = Mem::Read<int>(self + Addr::SilentAim::kCmdCount, -1);
                    NLOG("[silent] en=%d valid(target)=%d lmb=%d alive=%d armed=%d cmdCount=%d haveSeed=%d",
                         g_enabled.load() ? 1 : 0, g_valid.load() ? 1 : 0, lmb ? 1 : 0,
                         alive ? 1 : 0, armed ? 1 : 0, count, g_haveSeed.load() ? 1 : 0);
                }
            }

            if (armed)
            {
                ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), np, ny);
                if (std::isfinite(np) && std::isfinite(ny))
                {
                    if constexpr (AimFn::kPredictSpread)
                    {
                        const int tick = ClientTick(eng);
                        if (tick >= 0)
                        {
                            // seedBase выстрела = clientTick + глубина предсказания (live offset из hkSeed) + ручной тюнинг.
                            const std::uint32_t seedBase = static_cast<std::uint32_t>(
                                tick + g_tickOffset.load() + AimFn::kSeedTickOffset);
                            float cp, cy;
                            if (SolveAimAngle(base, pawn, seedBase, np, ny, cp, cy)) { np = cp; ny = cy; }
                        }
                    }
                    haveShot = true;
                    if (eng)
                    {
                        qa = FrameAngle(eng);
                        if (qa)
                        {
                            op = Mem::Read<float>(qa + 0);
                            oy = Mem::Read<float>(qa + 4);
                            Mem::Write<float>(qa + 0, np);
                            Mem::Write<float>(qa + 4, ny);
                            ringInjected = true;
                        }
                    }
                }
            }

            g_orig(thisptr, a2, a3);

            if (haveShot)
            {
                if (ringInjected)
                {
                    Mem::Write<float>(qa + 0, op);
                    Mem::Write<float>(qa + 4, oy);
                }
                Nemesis::PacketGuard::SanitizeCommands(self, np, ny);
                Nemesis::PacketGuard::SanitizeBaseAngle(self, np, ny);

                static DWORD s_wdbg = 0;
                if (GetTickCount() - s_wdbg > 400)
                {
                    s_wdbg = GetTickCount();
                    const float vp = base ? Mem::Read<float>(base + Client::dwViewAngles + 0) : 0.f;
                    const float vy = base ? Mem::Read<float>(base + Client::dwViewAngles + 4) : 0.f;
                    NLOG("[silent] WROTE aim(p=%.1f y=%.1f) view(p=%.1f y=%.1f) ring=%d", np, ny, vp, vy, ringInjected ? 1 : 0);
                }
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

    bool Enabled()
    {
        return g_enabled.load();
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

        g_seedTarget = reinterpret_cast<void*>(base + Addr::AimFn::fnSeedSha1);
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
