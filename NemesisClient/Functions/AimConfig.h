#pragma once

#include <cstdint>

struct AimConfig {
    bool   rageMode = false;        // true = Rage, false = Legit
    bool   aimbotEnabled = false;   // включен ли вообще
    float  aimbotFov = 5.0f;
    float  aimbotSmooth = 3.0f;
    int    aimbotBone = 0;          // 0=Head, 1=Chest, 2=Stomach
    bool   aimbotVisCheck = true;

    bool   triggerbotEnabled = false;
    float  triggerbotDelay = 50.0f; // миллисекунды
    bool   triggerbotHeadOnly = false;
    bool   triggerbotScopeOnly = true;
};

extern AimConfig g_AimConfig;