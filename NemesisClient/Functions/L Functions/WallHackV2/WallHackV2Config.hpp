#pragma once

#include <cstdint>

namespace Nemesis::WallHackV2::Config
{
    inline constexpr std::uint8_t kColorR = 40;
    inline constexpr std::uint8_t kColorG = 130;
    inline constexpr std::uint8_t kColorB = 255;
    inline constexpr std::uint8_t kColorA = 255;

    inline constexpr float kBloomColorR = 0.10f;
    inline constexpr float kBloomColorG = 0.45f;
    inline constexpr float kBloomColorB = 1.00f;

    inline constexpr float kBloomThreshold = 0.10f;
    inline constexpr float kBloomIntensity = 1.70f;
    inline constexpr float kBloomBlurRadius = 5.0f;
    inline constexpr float kBloomDownscale = 0.5f;

    inline constexpr float kBodyWidthScale  = 0.32f;
    inline constexpr float kHeadRadiusScale = 0.55f;

    inline constexpr float kChamsR = 0.12f;
    inline constexpr float kChamsG = 0.45f;
    inline constexpr float kChamsB = 1.00f;

    inline constexpr unsigned kChamsMinIndex = 900;
    inline constexpr unsigned kChamsMaxIndex = 60000;
}
