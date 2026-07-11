#pragma once

namespace Nemesis::SilentAim
{
    void Init();
    void Shutdown();
    void SetTarget(float x, float y, float z);   // мир: голова цели
    void ClearTarget();
}