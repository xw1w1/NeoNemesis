#pragma once

#include <cstddef>
#include <cstdint>

namespace Nemesis::Addresses
{
    namespace Modules
    {
        inline constexpr const char* kClient = "client.dll";
        inline constexpr const char* kEngine = "engine2.dll";
    }

    namespace Client
    {
        inline constexpr std::uintptr_t dwLocalPlayerPawn       = 0x2341698;
        inline constexpr std::uintptr_t dwLocalPlayerController = 0x2320720;
        inline constexpr std::uintptr_t dwViewAngles            = 0x23568C8;
        inline constexpr std::uintptr_t dwViewMatrix            = 0x2346B30;
        inline constexpr std::uintptr_t dwViewRender            = 0x2346EE0;
        inline constexpr std::uintptr_t dwEntityList            = 0x24E76A0;
    }

    namespace Schema
    {
        inline constexpr std::ptrdiff_t m_iHealth         = 0x34C;
        inline constexpr std::ptrdiff_t m_iTeamNum        = 0x3EB;
        inline constexpr std::ptrdiff_t m_pCameraServices = 0x1218;
        inline constexpr std::ptrdiff_t m_hPlayerPawn     = 0x90C;
        inline constexpr std::ptrdiff_t m_bPawnIsAlive    = 0x914;
    }

    namespace Sig
    {
        inline constexpr const char* kViewSetup = "";
    }

    namespace CameraView
    {
        inline constexpr float kDistance = 120.0f;
        inline constexpr float kHeight   = 25.0f;
        inline constexpr int   kToggleKey = 0x73;
    }
}
