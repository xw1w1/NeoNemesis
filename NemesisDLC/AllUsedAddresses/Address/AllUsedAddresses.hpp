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
        inline constexpr std::uintptr_t dwLocalPlayerPawn       = 0x2341528;
        inline constexpr std::uintptr_t dwLocalPlayerController = 0x2320570;
        inline constexpr std::uintptr_t dwViewAngles            = 0x2356748;
        inline constexpr std::uintptr_t dwViewMatrix            = 0x23469C0;
        inline constexpr std::uintptr_t dwEntityList            = 0x21D95E8;
        inline constexpr std::uintptr_t dwCameraManager         = 0x2079870;
        inline constexpr std::uintptr_t fnGetModelName          = 0x10BD860;
        inline constexpr std::uintptr_t fnSetModelString        = 0xC19940;
        inline constexpr std::uintptr_t fnReloadSubclass        = 0x1FAE80;
        inline constexpr std::uintptr_t fnSetPaintKit           = 0x8DEE60;
        inline constexpr std::uintptr_t fnGetRenderItemView     = 0x8BB840;
        inline constexpr std::uintptr_t fnApplyModel            = 0xC1B280;
        inline constexpr std::uintptr_t fnGetPaintKitId         = 0x105F9B0;
        inline constexpr std::uintptr_t dwButterflySubclass     = 0x22AB2F0;
        inline constexpr std::uintptr_t dwSubclassManager       = 0x21D9720;
        inline constexpr std::uintptr_t dwSvCheatsRef           = 0x233BEC0;
        inline constexpr std::uintptr_t dwSvCheatsConVar        = 0x233BEC8;
        inline constexpr std::uintptr_t dwSvCheatsRef2          = 0x2355FE0;
        inline constexpr std::uintptr_t fnResolveConVarValue    = 0x1826BA0;
        inline constexpr std::uintptr_t dwForceJump             = 0x2065FA0;
    }

    namespace Schema
    {
        inline constexpr std::ptrdiff_t m_iHealth         = 0x34C;
        inline constexpr std::ptrdiff_t m_iIDEntIndex     = 0x33FC;
        inline constexpr std::ptrdiff_t m_iTeamNum        = 0x3EB;
        inline constexpr std::ptrdiff_t m_pCameraServices = 0x1218;
        inline constexpr std::ptrdiff_t m_hPlayerPawn     = 0x90C;
        inline constexpr std::ptrdiff_t m_bPawnIsAlive    = 0x914;

        inline constexpr std::ptrdiff_t m_pWeaponServices       = 0x11E0;
        inline constexpr std::ptrdiff_t m_hMyWeapons            = 0x48;
        inline constexpr std::ptrdiff_t m_hActiveWeapon         = 0x60;
        inline constexpr std::ptrdiff_t m_hOwnerEntity          = 0x520;
        inline constexpr std::ptrdiff_t m_AttributeManager      = 0x1180;
        inline constexpr std::ptrdiff_t m_Item                  = 0x50;
        inline constexpr std::ptrdiff_t m_iItemDefinitionIndex  = 0x1BA;
        inline constexpr std::ptrdiff_t m_iEntityQuality        = 0x1BC;
        inline constexpr std::ptrdiff_t m_iItemIDHigh           = 0x1D0;
        inline constexpr std::ptrdiff_t m_iItemIDLow            = 0x1D4;
        inline constexpr std::ptrdiff_t m_iAccountID            = 0x1D8;
        inline constexpr std::ptrdiff_t m_steamID               = 0x780;
        inline constexpr std::ptrdiff_t m_bInitialized          = 0x1E8;
        inline constexpr std::ptrdiff_t m_nFallbackPaintKit     = 0x1658;
        inline constexpr std::ptrdiff_t m_nFallbackSeed         = 0x165C;
        inline constexpr std::ptrdiff_t m_flFallbackWear        = 0x1660;
        inline constexpr std::ptrdiff_t m_nFallbackStatTrak     = 0x1664;
        inline constexpr std::ptrdiff_t m_nSubclassID           = 0x380;
        inline constexpr std::ptrdiff_t m_pSubclassVData        = 0x388;
        inline constexpr std::ptrdiff_t m_pGameSceneNode        = 0x330;
        inline constexpr std::ptrdiff_t m_vecAbsOrigin          = 0xC8;
        inline constexpr std::ptrdiff_t m_vecViewOffset         = 0xE70;
        inline constexpr std::ptrdiff_t m_bModelDirty           = 0x11E2;
        inline constexpr std::ptrdiff_t m_bVisualsDataSet = 0x18B9;
        inline constexpr std::ptrdiff_t m_fFlags                = 0x3F8;
        inline constexpr std::ptrdiff_t m_vecAbsVelocity        = 0x3FC;
        inline constexpr std::ptrdiff_t m_vecVelocity           = 0x430;
        inline constexpr std::ptrdiff_t m_entitySpottedState    = 0x1C38;
        inline constexpr std::ptrdiff_t m_bSpotted              = 0x8;
    }

    namespace EconView
    {
        inline constexpr std::size_t    kPaintGetterIndex    = 13;
        inline constexpr std::size_t    kVisualsGetterIndex  = 12;
        inline constexpr std::ptrdiff_t kCompositePaintData  = 0x1F0;
    }

    namespace Subclass
    {
        inline constexpr std::ptrdiff_t kFlags      = 0x6C;
        inline constexpr std::ptrdiff_t kEntries    = 0x70;
        inline constexpr std::ptrdiff_t kCount      = 0x78;
        inline constexpr std::ptrdiff_t kCapacity   = 0x7C;
        inline constexpr std::ptrdiff_t kSlotStride = 0x18;
        inline constexpr std::ptrdiff_t kSlotKey    = 0x08;
        inline constexpr std::ptrdiff_t kSlotVData  = 0x10;
        inline constexpr std::ptrdiff_t kVDataCategory = 0x08;
        inline constexpr std::ptrdiff_t kVDataToken    = 0x0C;
        inline constexpr std::ptrdiff_t kVDataName     = 0x10;
    }

    namespace EntityList
    {
        inline constexpr std::uintptr_t kChunkStep   = 0x8;
        inline constexpr std::uintptr_t kChunkBase   = 0x10;
        inline constexpr std::uintptr_t kEntryStride = 0x70;
        inline constexpr std::uint32_t  kIndexMask   = 0x7FFF;
        inline constexpr std::uint32_t  kChunkShift  = 9;
        inline constexpr std::uint32_t  kSlotMask    = 0x1FF;
    }

    namespace CustomSkins
    {
        inline constexpr std::uint16_t kKnifeDefIndex = 515;
        inline constexpr int           kPaintKit      = 415;
        inline constexpr int           kSeed          = 0;
        inline constexpr float         kWear          = 0.0f;
        inline constexpr const char*   kSubclassName  = "weapon_knife_butterfly";
        inline constexpr std::uint32_t kTokenSeed     = 0x31415926;
    }

    namespace JumpBoost
    {
        inline constexpr int           kHoldKey      = 0x45;
        inline constexpr std::uint32_t kOnGroundFlag = 0x1;
        inline constexpr std::uint32_t kPress        = 65537;
        inline constexpr std::uint32_t kRelease      = 256;
        inline constexpr float         kBoost        = 1.08f;
        inline constexpr float         kMinSpeed     = 10.0f;
        inline constexpr float         kMaxSpeed     = 3400.0f;
        inline constexpr float         kBoostAdd     = 30.0f;
        inline constexpr int           kPollMs       = 1;
        inline constexpr int           kJumpDelayMs  = 30;
    }

    namespace ThirdpersonCam
    {
        inline constexpr std::ptrdiff_t kEnableFlag = 0x229;
        inline constexpr std::ptrdiff_t kAnglePitch = 0x230;
        inline constexpr std::ptrdiff_t kAngleYaw   = 0x234;
        inline constexpr std::ptrdiff_t kDistance   = 0x238;
    }

 

    namespace CameraView
    {
        inline constexpr int          kToggleKey  = 0x58;
        inline constexpr std::uint32_t kToggleScan = 0x2D;
        inline constexpr float        kDistance   = 120.0f;
    }

        namespace SilentAim
    {
        inline constexpr std::uintptr_t fnCreateMove = 0xC621D0; // CCSGOInput::CreateMove (хук)
        inline constexpr std::ptrdiff_t kCmdCount    = 0xBC8;    // int — число команд
        inline constexpr std::ptrdiff_t kCmdData     = 0xBD0;    // ptr — база вектора команд
        inline constexpr std::ptrdiff_t kCmdStride   = 0x60;     // РЕАЛЬНЫЙ страйд команды
        inline constexpr std::ptrdiff_t kViewAngle   = 0x10;     // viewangles QAngle в команде
    }

    namespace EngineInput
    {
        // источник eye-угла (engine2, sub_180075800): угол по двойной косвенности
        inline constexpr std::uintptr_t dwFrameCounter = 0x90B688; // dword счётчик кадра
        inline constexpr std::uintptr_t dwFrameRing    = 0x90C2B0; // кольцо 10 x 0x28
        inline constexpr std::ptrdiff_t kRecStride     = 0x28;
        inline constexpr std::ptrdiff_t kRecPtr        = 0x18;     // -> input-блок, QAngle @ +0
        inline constexpr int            kRingSize      = 10;
    }


    namespace Weapon
    {
        inline constexpr std::ptrdiff_t m_pVData            = 0x388;  // CCSWeaponBaseVData*
        inline constexpr std::ptrdiff_t m_nFireMode         = 0x17B8; // int 0/1
        inline constexpr std::ptrdiff_t m_fAccuracyPenalty  = 0x17D0; // float — текущая инаккураси (live!)
        inline constexpr std::ptrdiff_t m_iRecoilIndex      = 0x17DC; // int
        inline constexpr std::ptrdiff_t m_flRecoilIndex     = 0x17E0; // float (+1.0/выстрел)
        inline constexpr std::ptrdiff_t m_flNextClientFire  = 0x1908; // float
        // Поля внутри CCSWeaponBaseVData. CFiringModeFloat = [prim@0][sec@4], 8 байт.
        inline constexpr std::ptrdiff_t vd_flSpread           = 0x758;
        inline constexpr std::ptrdiff_t vd_flInaccuracyCrouch = 0x760;
        inline constexpr std::ptrdiff_t vd_flInaccuracyStand  = 0x768;
        inline constexpr std::ptrdiff_t vd_flInaccuracyJump   = 0x770;
        inline constexpr std::ptrdiff_t vd_flInaccuracyFire   = 0x788;
        inline constexpr std::ptrdiff_t vd_flInaccuracyMove   = 0x790;
        inline constexpr std::ptrdiff_t vd_nNumBullets        = 0x738; // int
    }
    namespace PawnCombat
    {
        inline constexpr std::ptrdiff_t m_iShotsFired                  = 0x1C64;
        inline constexpr std::ptrdiff_t m_bIsScoped                    = 0x1C50;
        inline constexpr std::ptrdiff_t m_zoomLevel                    = 0x1CB0;
        inline constexpr std::ptrdiff_t m_bFireBulletsSeedSynchronized = 0x955;
        inline constexpr std::ptrdiff_t m_pAimPunchServices            = 0x1490; // CCSPlayer_AimPunchServices*
        // Внутри AimPunchServices:
        inline constexpr std::ptrdiff_t ap_predictableBaseAngle        = 0x50;   // QAngle — боевой punch
        inline constexpr std::ptrdiff_t ap_predictableBaseAngleVel     = 0x5C;   // QAngle
        // m_fFlags = 0x3F8 уже есть в Schema (FL_ONGROUND = 0x1)
    }
    namespace AimFn
    {
        inline constexpr std::uintptr_t fnFireBullets      = 0xC81E30; // FX_FireBullets
        inline constexpr std::uintptr_t fnSpreadGen        = 0xC826A0; // пер-пелетный разброс
        inline constexpr std::uintptr_t fnSeedSha1         = 0xC81D80; // seed = SHA1(qPitch,qYaw,seedBase)[0]
        inline constexpr std::uintptr_t fnAngleQuantize    = 0xC7BEB0; // roundf(Norm(a)*2)*0.5
        inline constexpr std::uintptr_t fnGetAimPunchAngle = 0x7DB260; // GetAimPunchAngle(services,&out)
        inline constexpr std::uintptr_t fnWeaponFire       = 0x78F9D0; // функция выстрела оружия
        inline constexpr float          kRecoilScale = 2.0f;  // множитель punch (server default, сверить)
        inline constexpr std::uint32_t  kFlOnGround  = 0x1;
        inline constexpr float          kAngleGrid   = 0.5f;  // сетка квантайзера угла
    }
}
