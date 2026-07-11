#include "PacketGuard.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <cstdint>
#include <cmath>
#include <Windows.h>

namespace Nemesis::PacketGuard
{
    namespace Cfg = Nemesis::Addresses::PacketGuard;
    namespace SilentAim = Nemesis::Addresses::SilentAim;

    namespace
    {
        std::atomic<bool>  g_have{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        std::atomic<float> g_ax{ 0.f }, g_ay{ 0.f }, g_az{ 0.f };

        inline bool HeapPtr(std::uintptr_t p)
        {
            return p >= Cfg::kHeapMin && p <= Cfg::kHeapMax;
        }

        inline bool NonZeroVec(float x, float y, float z)
        {
            return (x * x + y * y + z * z) > Cfg::kNonZeroVecSq;
        }
    }

    void SetTarget(float hx, float hy, float hz, float ax, float ay, float az)
    {
        g_hx.store(hx); g_hy.store(hy); g_hz.store(hz);
        g_ax.store(ax); g_ay.store(ay); g_az.store(az);
        g_have.store(true);
    }

    void ClearTarget()
    {
        g_have.store(false);
    }

    void SanitizeCommands(std::uintptr_t self, float pitch, float yaw)
    {
        if (!g_have.load()) return;
        if (!std::isfinite(pitch) || !std::isfinite(yaw)) return;

        const int count = Mem::Read<int>(self + SilentAim::kCmdCount, 0);
        const std::uintptr_t data = Mem::Read<std::uintptr_t>(self + SilentAim::kCmdData);
        if (count <= 0 || count > SilentAim::kCmdCountMax || !HeapPtr(data)) return;

        const float hx = g_hx.load(), hy = g_hy.load(), hz = g_hz.load();
        const float ax = g_ax.load(), ay = g_ay.load(), az = g_az.load();

        for (int i = 0; i < count; ++i)
        {
            const std::uintptr_t e = data + static_cast<std::uintptr_t>(i) * SilentAim::kCmdStride;
            if (!HeapPtr(e)) break;

            Mem::Write<float>(e + SilentAim::kViewAngle + SilentAim::kAnglePitch, pitch);
            Mem::Write<float>(e + SilentAim::kViewAngle + SilentAim::kAngleYaw, yaw);

            const float th = Mem::Read<float>(e + SilentAim::kCmdTargetHead + 0);
            const float ti = Mem::Read<float>(e + SilentAim::kCmdTargetHead + 4);
            const float tj = Mem::Read<float>(e + SilentAim::kCmdTargetHead + 8);
            if (!NonZeroVec(th, ti, tj)) continue;

            Mem::Write<float>(e + SilentAim::kCmdTargetHead + 0, hx);
            Mem::Write<float>(e + SilentAim::kCmdTargetHead + 4, hy);
            Mem::Write<float>(e + SilentAim::kCmdTargetHead + 8, hz);

            Mem::Write<float>(e + SilentAim::kCmdTargetAbs + 0, ax);
            Mem::Write<float>(e + SilentAim::kCmdTargetAbs + 4, ay);
            Mem::Write<float>(e + SilentAim::kCmdTargetAbs + 8, az);
        }
    }

    void SanitizeBaseAngle(std::uintptr_t self, float pitch, float yaw)
    {
        if (!g_have.load()) return;
        if (!std::isfinite(pitch) || !std::isfinite(yaw)) return;

        Mem::Write<float>(self + SilentAim::kBaseAngleA + SilentAim::kAnglePitch, pitch);
        Mem::Write<float>(self + SilentAim::kBaseAngleA + SilentAim::kAngleYaw, yaw);
        Mem::Write<float>(self + SilentAim::kBaseAngleB + SilentAim::kAnglePitch, pitch);
        Mem::Write<float>(self + SilentAim::kBaseAngleB + SilentAim::kAngleYaw, yaw);

        if constexpr (Cfg::kWriteBasePtrA)
        {
            const std::uintptr_t pa = Mem::Read<std::uintptr_t>(self + SilentAim::kBasePtrA);
            if (HeapPtr(pa))
            {
                Mem::Write<float>(pa + SilentAim::kBasePtrAInner + SilentAim::kAnglePitch, pitch);
                Mem::Write<float>(pa + SilentAim::kBasePtrAInner + SilentAim::kAngleYaw, yaw);
            }
        }

        if constexpr (Cfg::kWriteBasePtrB)
        {
            const std::uintptr_t pb = Mem::Read<std::uintptr_t>(self + SilentAim::kBasePtrB);
            if (HeapPtr(pb))
            {
                Mem::Write<float>(pb + SilentAim::kBasePtrBInner + SilentAim::kAnglePitch, pitch);
                Mem::Write<float>(pb + SilentAim::kBasePtrBInner + SilentAim::kAngleYaw, yaw);
            }
        }
    }

    void DiagBaseAngle(std::uintptr_t self, float realPitch, float realYaw)
    {
        static DWORD last = 0;
        const DWORD now = GetTickCount();
        if (now - last < Cfg::kDiagInlineMs) return;
        if (std::fabs(realPitch) < Cfg::kDiagAngMin && std::fabs(realYaw) < Cfg::kDiagAngMin) return;

        int hits = 0;
        for (std::ptrdiff_t off = 0; off <= Cfg::kDiagSelfSpan && hits < Cfg::kDiagInlineHits; off += 4)
        {
            const float p = Mem::Read<float>(self + off + SilentAim::kAnglePitch);
            const float y = Mem::Read<float>(self + off + SilentAim::kAngleYaw);
            if (std::fabs(p - realPitch) < Cfg::kDiagAngTol && std::fabs(y - realYaw) < Cfg::kDiagAngTol)
            {
                NLOG("[diag] base? off=0x%llX  p=%.2f y=%.2f", (unsigned long long)off, p, y);
                ++hits;
            }
        }
        last = now;
    }

    void DiagBasePtr(std::uintptr_t self, float realPitch, float realYaw)
    {
        static DWORD last = 0;
        const DWORD now = GetTickCount();
        if (now - last < Cfg::kDiagPtrMs) return;
        if (std::fabs(realPitch) < Cfg::kDiagAngMin && std::fabs(realYaw) < Cfg::kDiagAngMin) return;
        last = now;

        auto match = [&](std::uintptr_t addr) -> bool
        {
            const float p = Mem::Read<float>(addr + SilentAim::kAnglePitch);
            const float y = Mem::Read<float>(addr + SilentAim::kAngleYaw);
            return std::fabs(p - realPitch) < Cfg::kDiagAngTol && std::fabs(y - realYaw) < Cfg::kDiagAngTol;
        };

        int hits = 0;
        for (std::ptrdiff_t off = 0; off <= Cfg::kDiagSelfSpan && hits < Cfg::kDiagPtrHits; off += 8)
        {
            const std::uintptr_t p = Mem::Read<std::uintptr_t>(self + off);
            if (!HeapPtr(p)) continue;
            if (p >= self && p <= self + Cfg::kDiagSelfSpan) continue;

            for (std::ptrdiff_t in = 0; in <= Cfg::kDiagP1Span && hits < Cfg::kDiagPtrHits; in += 4)
            {
                if (match(p + in))
                {
                    NLOG("[diag2] P1 self+0x%llX -> +0x%llX",
                         (unsigned long long)off, (unsigned long long)in);
                    ++hits;
                }
            }

            for (std::ptrdiff_t jo = 0; jo <= Cfg::kDiagP2PtrSpan && hits < Cfg::kDiagPtrHits; jo += 8)
            {
                const std::uintptr_t q = Mem::Read<std::uintptr_t>(p + jo);
                if (!HeapPtr(q)) continue;

                for (std::ptrdiff_t in = 0; in <= Cfg::kDiagP2Span && hits < Cfg::kDiagPtrHits; in += 4)
                {
                    if (match(q + in))
                    {
                        NLOG("[diag2] P2 self+0x%llX -> +0x%llX -> +0x%llX",
                             (unsigned long long)off, (unsigned long long)jo,
                             (unsigned long long)in);
                        ++hits;
                    }
                }
            }
        }

        if (hits == 0)
            NLOG("[diag2] no pointer-reachable base angle found");
    }
}
