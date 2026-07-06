#include "JumpSystem.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
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

        void WriteJump(std::uintptr_t base, std::uint32_t state)
        {
            Mem::Write<std::uint32_t>(base + Client::dwForceJump, state);
        }

        float HorizSpeed(std::uintptr_t vel)
        {
            const float vx = Mem::Read<float>(vel + 0x0);
            const float vy = Mem::Read<float>(vel + 0x4);
            return std::sqrt(vx * vx + vy * vy);
        }

        // Руление: боковой ввод в сторону поворота мыши — движок сам air-accelerate'ит.
        void AutoStrafe(std::uintptr_t ms, float viewYaw, float prevYaw)
        {
            float dy = viewYaw - prevYaw;
            while (dy >  180.0f) dy -= 360.0f;
            while (dy < -180.0f) dy += 360.0f;

            float side = 0.0f;
            if (dy > JumpBoost::kYawDeadzone)
                side = JumpBoost::kStrafeMove;      // поворот влево -> стрейф влево
            else if (dy < -JumpBoost::kYawDeadzone)
                side = -JumpBoost::kStrafeMove;     // поворот вправо -> стрейф вправо

            Mem::Write<float>(ms + Schema::ms_flForwardMove, 0.0f);
            Mem::Write<float>(ms + Schema::ms_flLeftMove, side);
        }

        void Worker()
        {
            using clock = std::chrono::steady_clock;

            bool  wasHeld    = false;
            bool  prevGround = true;
            bool  jumped     = false;
            float prevYaw    = 0.0f;
            float targetSpeed = 0.0f;
            auto  landedAt   = clock::now();

            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                const bool held = base
                    && GameInForeground()
                    && (GetAsyncKeyState(JumpBoost::kHoldKey) & 0x8000);

                if (!held)
                {
                    if (wasHeld && base)
                        WriteJump(base, JumpBoost::kRelease);
                    wasHeld     = false;
                    jumped      = false;
                    prevGround  = true;
                    targetSpeed = 0.0f;
                    std::this_thread::sleep_for(std::chrono::milliseconds(JumpBoost::kPollMs));
                    continue;
                }

                const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
                if (!pawn || Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
                {
                    wasHeld = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(JumpBoost::kPollMs));
                    continue;
                }

                const std::uintptr_t vel = pawn + Schema::m_vecVelocity;
                const std::uintptr_t ms  = Mem::Read<std::uintptr_t>(pawn + Schema::m_pMovementServices);
                const std::uint32_t flags = Mem::Read<std::uint32_t>(pawn + Schema::m_fFlags);
                const bool onGround = (flags & JumpBoost::kOnGroundFlag) != 0;
                const auto now = clock::now();
                const float viewYaw = Mem::Read<float>(base + Client::dwViewAngles + JumpBoost::kViewYawOff);
                const float cur = HorizSpeed(vel);

                if (!wasHeld)
                {
                    landedAt    = now;
                    jumped      = false;
                    prevGround  = onGround;
                    prevYaw     = viewYaw;
                    targetSpeed = cur;
                }

                if (HeapPtr(ms))
                    Mem::Write<float>(ms + Schema::ms_flMaxspeed, JumpBoost::kForceMaxspeed);

                if (!onGround && prevGround)
                {
                    if (targetSpeed < cur)
                        targetSpeed = cur;
                    targetSpeed += JumpBoost::kBoostAdd;
                    if (targetSpeed > JumpBoost::kMaxSpeed)
                        targetSpeed = JumpBoost::kMaxSpeed;
                }
                else if (onGround && !prevGround)
                {
                    landedAt = now;
                    jumped   = false;
                }

                if (!onGround && HeapPtr(ms))
                    AutoStrafe(ms, viewYaw, prevYaw);

                // Пол скорости: подхватываем магнитуду, когда просела (лестницы/приземление/тейкофф),
                // сохраняя направление (руление остаётся за движком).
                if (cur > targetSpeed)
                    targetSpeed = cur;
                if (cur < targetSpeed && cur > JumpBoost::kMinSpeed)
                {
                    const float scale = targetSpeed / cur;
                    Mem::Write<float>(vel + 0x0, Mem::Read<float>(vel + 0x0) * scale);
                    Mem::Write<float>(vel + 0x4, Mem::Read<float>(vel + 0x4) * scale);
                }

                prevYaw = viewYaw;

                if (onGround)
                {
                    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - landedAt).count();

                    if (jumped)
                        WriteJump(base, JumpBoost::kPress);
                    else if (waited >= JumpBoost::kJumpDelayMs)
                    {
                        WriteJump(base, JumpBoost::kPress);
                        jumped = true;
                    }
                    else
                        WriteJump(base, JumpBoost::kRelease);
                }
                else
                {
                    WriteJump(base, JumpBoost::kRelease);
                }

                prevGround = onGround;
                wasHeld    = true;

                std::this_thread::sleep_for(std::chrono::milliseconds(JumpBoost::kPollMs));
            }

            const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
            if (base)
                WriteJump(base, JumpBoost::kRelease);
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
}
