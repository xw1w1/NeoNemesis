#include "JumpSystem.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <Windows.h>

namespace Nemesis::JumpSystem
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::atomic<bool> g_running{ false };
        std::thread       g_worker;

        bool HeapPtr(std::uintptr_t p)
        {
            return p >= 0x10000 && p <= 0x7FFFFFFFFFFFull;
        }

        bool GameInForeground()
        {
            HWND hwnd = GetForegroundWindow();
            if (!hwnd)
                return false;
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            return pid == GetCurrentProcessId();
        }

        bool HoldKeyDown()
        {
            return GameInForeground() && (GetAsyncKeyState(JumpBoost::kHoldKey) & 0x8000);
        }

        // Прыжок и стрейф теперь тик-точно в JumpTick (исходящая команда). Поток простаивает.
        void Worker()
        {
            while (g_running.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(JumpBoost::kPollMs));
        }
    }

    void JumpTick(std::uintptr_t base)
    {
        if (!base)
            return;

        static float s_prevYaw = 0.0f;
        static bool  s_wasHeld = false;

        if (!HoldKeyDown())
        {
            Mem::Write<std::uint32_t>(base + Client::dwForceJump, JumpBoost::kRelease);
            s_wasHeld = false;
            return;
        }

        const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
        if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
        {
            Mem::Write<std::uint32_t>(base + Client::dwForceJump, JumpBoost::kRelease);
            s_wasHeld = false;
            return;
        }

        const std::uint32_t flags = Mem::Read<std::uint32_t>(pawn + Schema::m_fFlags);
        const bool onGround = (flags & JumpBoost::kOnGroundFlag) != 0;

        // Тик-точно: на земле — прыжок в эту команду, в воздухе — отпускаем (даёт фронт для реджампа).
        Mem::Write<std::uint32_t>(base + Client::dwForceJump,
                                  onGround ? JumpBoost::kPress : JumpBoost::kRelease);

        // Разгонный авто-стрейф В ЭТУ ЖЕ КОМАНДУ (авторитетно на dedicated, как прыжок).
        const std::uintptr_t ms = Mem::Read<std::uintptr_t>(pawn + Schema::m_pMovementServices);
        const float viewYaw = Mem::Read<float>(base + Client::dwViewAngles + JumpBoost::kViewYawOff);
        if (!s_wasHeld)
            s_prevYaw = viewYaw;

        if (!onGround && HeapPtr(ms))
        {
            float dy = viewYaw - s_prevYaw;
            while (dy >  180.0f) dy -= 360.0f;
            while (dy < -180.0f) dy += 360.0f;

            float side = 0.0f;
            if (dy > JumpBoost::kYawDeadzone)
                side = JumpBoost::kStrafeMove;
            else if (dy < -JumpBoost::kYawDeadzone)
                side = -JumpBoost::kStrafeMove;

            Mem::Write<float>(ms + Schema::ms_flForwardMove, 0.0f);
            Mem::Write<float>(ms + Schema::ms_flLeftMove, side);
        }

        s_prevYaw = viewYaw;
        s_wasHeld = true;
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
}
