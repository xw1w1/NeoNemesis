#include "PacketGuard.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include <atomic>
#include <cstdint>
#include <cmath>

namespace Nemesis::PacketGuard
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::atomic<bool>  g_have{ false };
        std::atomic<float> g_hx{ 0.f }, g_hy{ 0.f }, g_hz{ 0.f };
        std::atomic<float> g_ax{ 0.f }, g_ay{ 0.f }, g_az{ 0.f };

        inline bool HeapPtr(std::uintptr_t p)
        {
            return p >= 0x10000 && p <= 0x7FFFFFFFFFFFull;
        }

        inline bool NonZeroVec(float x, float y, float z)
        {
            return (x * x + y * y + z * z) > 1.0f;
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
        if (count <= 0 || count > 150 || !HeapPtr(data)) return;

        const float hx = g_hx.load(), hy = g_hy.load(), hz = g_hz.load();
        const float ax = g_ax.load(), ay = g_ay.load(), az = g_az.load();

        for (int i = 0; i < count; ++i)
        {
            const std::uintptr_t e = data + static_cast<std::uintptr_t>(i) * SilentAim::kCmdStride;
            if (!HeapPtr(e)) break;

            Mem::Write<float>(e + SilentAim::kViewAngle + 0, pitch);
            Mem::Write<float>(e + SilentAim::kViewAngle + 4, yaw);

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
}
