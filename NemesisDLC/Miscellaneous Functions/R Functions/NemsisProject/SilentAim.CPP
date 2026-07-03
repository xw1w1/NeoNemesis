#include "SilentAim.hpp"
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
        constexpr std::uintptr_t kCreateMove = 0xC621D0;
        constexpr float kRad2Deg = 57.2957795f;
        using CreateMoveFn = void(__fastcall*)(void*, int, void*);
        CreateMoveFn g_orig = nullptr;

        std::atomic<bool>  g_valid{ false };
        std::atomic<bool>  g_enabled{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        bool g_loggedHook = false;

        bool g_loggedInject = false;

        inline bool SameBits(float a, float b)
        {
            return *reinterpret_cast<std::uint32_t*>(&a) ==
                   *reinterpret_cast<std::uint32_t*>(&b);
        }

        // ищем в блоке два подряд float == (p0,y0) — копию угла команды —
        // и переписываем их на угол в голову (np,ny). dwViewAngles НЕ трогаем.
        void OverwriteMatches(std::uintptr_t block, std::size_t size,
                              float p0, float y0, float np, float ny, int& hits)
        {
            if (block < 0x10000) return;
            for (std::size_t off = 0; off + 8 <= size; off += 4)
            {
                if (!SameBits(Mem::Read<float>(block + off), p0)) continue;
                if (!SameBits(Mem::Read<float>(block + off + 4), y0)) continue;
                Mem::Write<float>(block + off + 0, np);
                Mem::Write<float>(block + off + 4, ny);
                ++hits;
            }
        }

        // a3 = команда/контекст: скан самой структуры + один уровень указателей
        int InjectIntoCommand(void* a3, float p0, float y0, float np, float ny)
        {
            const std::uintptr_t cmd = reinterpret_cast<std::uintptr_t>(a3);
            if (!cmd) return 0;

            int hits = 0;
            OverwriteMatches(cmd, 0x400, p0, y0, np, ny, hits);

            for (std::size_t off = 0; off < 0x200; off += 8)
            {
                const std::uintptr_t p = Mem::Read<std::uintptr_t>(cmd + off);
                if (p < 0x10000 || p > 0x7FFFFFFFFFFFull) continue;
                OverwriteMatches(p, 0x200, p0, y0, np, ny, hits);
            }
            return hits;
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
            // F6 — тоггл silent
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

            if (!g_loggedHook && base)
            {
                g_loggedHook = true;
                NLOG("[silent] CreateMove hook alive, this=%p", thisptr);
            }

            // сначала оригинал строит команду с углом игрока
            g_orig(thisptr, a2, a3);

            if (!base || !g_enabled.load() || !g_valid.load())
                return;

            // только в момент выстрела (зажата ЛКМ)
            if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
                return;

            const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
            if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
                return;
           
            float np = 0.f, ny = 0.f;
            ComputeAngle(pawn, g_hx.load(), g_hy.load(), g_hz.load(), np, ny);

            // реальный угол = то, что g_orig только что записал (== копии в команде)
            const std::uintptr_t va = base + Client::dwViewAngles;
            const float p0 = Mem::Read<float>(va + 0);
            const float y0 = Mem::Read<float>(va + 4);

            // пишем угол ТОЛЬКО в команду (a3), камеру не трогаем -> silent
            const int hits = InjectIntoCommand(a3, p0, y0, np, ny);
            if (!g_loggedInject)
            {
                g_loggedInject = true;
                NLOG("[silent] cmd angle hits=%d", hits);
            }

        }
       
    }

    void SetTarget(float x, float y, float z)
    {
        g_hx.store(x);
        g_hy.store(y);
        g_hz.store(z);
        g_valid.store(true);
    }

    void ClearTarget()
    {
        g_valid.store(false);
    }

    void Init()
    {
        MH_Initialize();   // если уже инициализирован другим модулем — вернёт ALREADY_INITIALIZED, ок

        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base)
        {
            NERR("[silent] client.dll not found");
            return;
        }

        void* target = reinterpret_cast<void*>(base + kCreateMove);
        if (MH_CreateHook(target, &hkCreateMove, reinterpret_cast<void**>(&g_orig)) != MH_OK)
        {
            NERR("[silent] CreateHook failed");
            return;
        }
        if (MH_EnableHook(target) != MH_OK)
        {
            NERR("[silent] EnableHook failed");
            return;
        }
        NLOG("[silent] hooked CreateMove @ client+0x%llX (F6 to toggle)", (unsigned long long)kCreateMove);
    }

    void Shutdown()
    {
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (base)
            MH_DisableHook(reinterpret_cast<void*>(base + kCreateMove));
    }
}