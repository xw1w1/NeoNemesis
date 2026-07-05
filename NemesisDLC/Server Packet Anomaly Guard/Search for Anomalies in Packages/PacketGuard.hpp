#pragma once

#include <cstdint>

namespace Nemesis::PacketGuard
{
    void SetTarget(float hx, float hy, float hz, float ax, float ay, float az);
    void ClearTarget();
    void SanitizeCommands(std::uintptr_t self, float pitch, float yaw);
}
