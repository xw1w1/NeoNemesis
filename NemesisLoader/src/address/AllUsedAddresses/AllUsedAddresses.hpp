#pragma once
#include <cstdint>

//
// Central table of CS2 memory offsets used by NemesisLoader.
// Source: cs2-dumper (https://github.com/a2x/cs2-dumper).
// Update these on every CS2 patch instead of editing call sites.
//
namespace Addr
{
    // Offsets relative to client.dll module base.
    namespace Client
    {
        constexpr std::uintptr_t dwLocalPlayerController = 0x2320720;
        constexpr std::uintptr_t dwLocalPlayerPawn       = 0x2341698;
        constexpr std::uintptr_t dwEntityList            = 0x24E76A0;
    }

    // Field offsets inside CCSPlayerController.
    namespace PlayerController
    {
        constexpr std::uintptr_t m_iszPlayerName            = 0x6F4;  // char[128]
        constexpr std::uintptr_t m_sSanitizedPlayerName     = 0x860;  // CUtlString
        constexpr std::uintptr_t m_pActionTrackingServices  = 0x818;  // CCSPlayerController_ActionTrackingServices*
    }

    // Field offsets inside CCSPlayerController_ActionTrackingServices.
    namespace ActionTracking
    {
        constexpr std::uintptr_t m_iKills  = 0x30; // int32
        constexpr std::uintptr_t m_iDeaths = 0x34; // int32
    }

    // Offsets relative to soundsystem.dll module base.
    // Source: manual reversing of soundsystem.dll (Jun 2026 build).
    namespace SoundSystem
    {
        // CSoundOpSystem::StartSoundEvent(void* this, const char* eventName)
        // Win64 ABI: RCX=this, RDX=eventName
        // First 5 bytes of prologue ("mov [rsp+10], rdx") are non-RIP-relative,
        // safe to relocate into a trampoline.
        constexpr std::uintptr_t StartSoundEvent = 0x79DD0;
    }
}
