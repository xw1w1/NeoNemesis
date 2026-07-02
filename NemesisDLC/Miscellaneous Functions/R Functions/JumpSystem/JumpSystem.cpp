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

        void BoostTakeoff(std::uintptr_t pawn, float& targetSpeed, std::uint64_t n)
        {
            const std::uintptr_t vel = pawn + Schema::m_vecVelocity;
            const float vx = Mem::Read<float>(vel);
            const float vy = Mem::Read<float>(vel + 0x4);

            const float speed = std::sqrt(vx * vx + vy * vy);
            if (speed < JumpBoost::kMinSpeed)
            {
                NLOG("[jump] #%llu speed=%.1f (slow, no dir)", (unsigned long long)n, speed);
                return;
            }

            if (targetSpeed < speed)
                targetSpeed = speed;              // разогнался сам - подхватываем

            targetSpeed += JumpBoost::kBoostAdd;  // +30 накопительно
            if (targetSpeed > JumpBoost::kMaxSpeed)
                targetSpeed = JumpBoost::kMaxSpeed;

            const float scale = targetSpeed / speed;
            Mem::Write<float>(vel, vx * scale);
            Mem::Write<float>(vel + 0x4, vy * scale);

            NLOG("[jump] #%llu %.1f -> %.1f (+%.0f)",
                 (unsigned long long)n, speed, targetSpeed, JumpBoost::kBoostAdd);
        }

        void Worker()
        {
            using clock = std::chrono::steady_clock;

            bool  wasHeld    = false;
            bool  prevGround = true;
            bool  jumped     = false;
            float targetSpeed = 0.0f;
            auto  landedAt   = clock::now();
            std::uint64_t jumpCount = 0;

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
                    targetSpeed = 0.0f;           // сброс накопления
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

                const std::uint32_t flags = Mem::Read<std::uint32_t>(pawn + Schema::m_fFlags);
                const bool onGround = (flags & JumpBoost::kOnGroundFlag) != 0;
                const auto now = clock::now();

                if (!wasHeld)
                {
                    landedAt   = now;
                    jumped     = false;
                    prevGround = onGround;
                }

                if (onGround && !prevGround)
                {
                    landedAt = now;
                    jumped   = false;
                }
                else if (!onGround && prevGround)
                {
                    if (jumped)
                        BoostTakeoff(pawn, targetSpeed, ++jumpCount);
                }

                if (onGround)
                {
                    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - landedAt).count();

                    if (!jumped && waited >= JumpBoost::kJumpDelayMs)
                    {
                        WriteJump(base, JumpBoost::kPress);
                        jumped = true;
                    }
                    else if (!jumped)
                        WriteJump(base, JumpBoost::kRelease);
                    else
                        WriteJump(base, JumpBoost::kPress);
                }
                else
                {
                    WriteJump(base, JumpBoost::kRelease);
                }

                prevGround = onGround;
                wasHeld    = true;

                std::this_thread::sleep_for(std::chrono::milliseconds(JumpBoost::kPollMs));
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
}