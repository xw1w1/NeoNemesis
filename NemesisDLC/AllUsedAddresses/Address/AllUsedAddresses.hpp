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

        inline constexpr std::ptrdiff_t m_pObserverServices = 0x11F8;
        inline constexpr std::ptrdiff_t obs_iObserverMode   = 0x48;
        inline constexpr std::ptrdiff_t obs_hObserverTarget = 0x4C;

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
        // CPlayer_MovementServices — аналоговый ввод движения (для авто-стрейфа)
        inline constexpr std::ptrdiff_t m_pMovementServices     = 0x1220; // pawn -> services
        inline constexpr std::ptrdiff_t ms_flMaxspeed           = 0x1AC;
        inline constexpr std::ptrdiff_t ms_flForwardMove        = 0x1C0;
        inline constexpr std::ptrdiff_t ms_flLeftMove           = 0x1C4;
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
        // Bhop + авто-стрейф ТОЛЬКО через ввод (dwForceJump + аналоговый move).
        // БЕЗ записей velocity/maxspeed: они расходятся с сервером -> откаты («затупы»).
        inline constexpr int           kHoldKey      = 0x5A;   // Z
        inline constexpr std::uint32_t kOnGroundFlag = 0x1;    // FL_ONGROUND
        inline constexpr std::uint32_t kPress        = 65537;  // dwForceJump: прыжок зажат
        inline constexpr std::uint32_t kRelease      = 256;    // dwForceJump: отпущен
        inline constexpr int           kPollMs       = 1;
        inline constexpr int           kJumpDelayMs  = 0;      // мгновенный реджамп (release уже есть в воздухе)
        inline constexpr std::ptrdiff_t kViewYawOff  = 0x4;    // yaw в QAngle dwViewAngles
        inline constexpr float         kStrafeMove   = 10000.0f; // сила бок. ввода (движок клампит к maxspeed)
        inline constexpr float         kYawDeadzone  = 0.01f;  // мертвая зона поворота мыши — меньше = резче
    }

    namespace AimMode
    {
        inline constexpr int kToggleKey = 0x79;   // F10 — переключить рендер/аим Rage<->Legit
        inline constexpr int kStartRage = 0;      // старт: 0 = Legit, 1 = Rage
    }

    namespace NightMode
    {
        inline constexpr int      kToggleKey = 0x78;   // F9
        // Тёмно-синий полноэкранный пост-тинт (ImGui overlay). kTintA = сила ночи (0..255).
        inline constexpr unsigned kTintR = 6;
        inline constexpr unsigned kTintG = 12;
        inline constexpr unsigned kTintB = 40;
        inline constexpr unsigned kTintA = 140;
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
        inline constexpr std::ptrdiff_t kCmdShootPos   = 0x1C;   // shoot_position (не трогаем)
        inline constexpr std::ptrdiff_t kCmdTargetHead = 0x28;   // target_head_position_check
        inline constexpr std::ptrdiff_t kCmdTargetAbs  = 0x34;   // target_abs_position_check
        inline constexpr std::ptrdiff_t kBaseAngleA    = 0x2A0;  // eye/base viewangles (sub_180225A10)
        inline constexpr std::ptrdiff_t kBaseAngleB    = 0x758;  // вторая копия текущего угла
        inline constexpr std::ptrdiff_t kCameraAngle   = 0x688;  // камера — НЕ трогать
        // base.viewangles за указателем (heap, найдено DiagBasePtr) — исходящий пакет читает отсюда
        inline constexpr std::ptrdiff_t kBasePtrA      = 0xB58;  // self+0xB58 -> (+0x48) QAngle
        inline constexpr std::ptrdiff_t kBasePtrAInner = 0x48;
        inline constexpr std::ptrdiff_t kBasePtrB      = 0xFF8;  // self+0xFF8 -> (+0x27C) QAngle
        inline constexpr std::ptrdiff_t kBasePtrBInner = 0x27C;
        inline constexpr std::ptrdiff_t kAnglePitch    = 0x0;    // QAngle: pitch
        inline constexpr std::ptrdiff_t kAngleYaw      = 0x4;    // QAngle: yaw
        inline constexpr int            kCmdCountMax   = 150;    // sanity-лимит числа команд
    }

    namespace PacketGuard
    {
        // границы валидного heap-указателя
        inline constexpr std::uintptr_t kHeapMin        = 0x10000;
        inline constexpr std::uintptr_t kHeapMax        = 0x7FFFFFFFFFFFull;
        inline constexpr float          kNonZeroVecSq   = 1.0f;   // порог «вектор не нулевой» (кв.)

        // тумблеры записи копий base.viewangles (обе ВКЛ; выключать после обновы для реврайза)
        inline constexpr bool           kWriteBasePtrA  = true;   // self+0xB58->+0x48
        inline constexpr bool           kWriteBasePtrB  = true;   // self+0xFF8->+0x27C

        // параметры диагностик-сканов (DiagBaseAngle / DiagBasePtr) — нужны после обновления игры
        inline constexpr std::ptrdiff_t kDiagSelfSpan   = 0x1200; // сколько байт self сканить
        inline constexpr std::ptrdiff_t kDiagP1Span     = 0x400;  // pointee 1-го уровня
        inline constexpr std::ptrdiff_t kDiagP2PtrSpan  = 0x180;  // где искать под-указатели
        inline constexpr std::ptrdiff_t kDiagP2Span     = 0x40;   // pointee 2-го уровня
        inline constexpr float          kDiagAngTol     = 0.3f;   // допуск совпадения угла
        inline constexpr float          kDiagAngMin     = 0.05f;  // ниже — угол «нулевой», скип
        inline constexpr unsigned       kDiagInlineMs   = 400;    // rate-limit DiagBaseAngle
        inline constexpr unsigned       kDiagPtrMs      = 700;    // rate-limit DiagBasePtr
        inline constexpr int            kDiagInlineHits = 24;     // cap хитов DiagBaseAngle
        inline constexpr int            kDiagPtrHits    = 40;     // cap хитов DiagBasePtr
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
