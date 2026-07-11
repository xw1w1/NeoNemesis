#pragma once

#include <cstdint>

namespace Nemesis::JumpSystem
{
    void Start();
    void Stop();

    // Тик-точный bhop: вызывать из хука CreateMove (на игровом потоке, до g_orig).
    // Ставит прыжок в исходящую команду -> сервер видит -> работает в матче.
    void JumpTick(std::uintptr_t base);
}
