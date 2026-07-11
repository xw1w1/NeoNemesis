#pragma once

// ============================================================================
// Конфиг легит-бота. Меняй значения и пересобирай.
// ============================================================================
namespace Nemesis::LegitBot::Config
{
    // Части тела, по которым целится бот (1 точка на часть):
    inline constexpr bool  aimHead    = true;
    inline constexpr bool  aimChest   = true;
    inline constexpr bool  aimStomach = true;
    inline constexpr bool  aimHands   = false;  // выкл (не удалять)
    inline constexpr bool  aimLegs    = false;  // выкл (не удалять)

    // Круг FOV (радиус в пикселях): цель берётся только внутри него
    inline constexpr float fovRadius = 90.0f;

    // Скорость наводки камеры в градусах/сек (кадронезависимо, потолок). Супер-быстро.
    inline constexpr float aimSpeed = 3000.0f;

    // Плавность ease-out (замедление у цели): больше = резче/быстрее сходится, меньше = мягче.
    inline constexpr float aimSmooth = 60.0f;

    // Задержка реакции: 0 = моментально стреляет по захвату цели.
    inline constexpr float reactionMs = 0.0f;

    // Сколько выстрелов делать очередью, потом ждать сброса отдачи.
    inline constexpr int   burstShots = 3;

    // Порог авто-огня (px): стреляет только когда прицел так близко к точке.
    inline constexpr float fireDist = 2.5f;
}
