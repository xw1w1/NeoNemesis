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
        // === Глобалы из дампера (билд 14169, sezzyaep/CS2-OFFSETS / ExitScam Dumper) ===
        inline constexpr std::uintptr_t dwLocalPlayerPawn = 0x23A4238;
        inline constexpr std::uintptr_t dwLocalPlayerController = 0x237EBA0;
        inline constexpr std::uintptr_t dwViewAngles = 0x23B9C78;
        inline constexpr std::uintptr_t dwViewMatrix = 0x23A9340;
        inline constexpr std::uintptr_t dwEntityList = 0x254EE60;
        inline constexpr std::uintptr_t dwCSGOInput = 0x23B95F0;

        // === НЕ В ДАМПЕРЕ — пересверить в MyGame.asm / pattern scan (значения ниже устарели, билд 14166/14168) ===
        // Эти адреса (fn*, dwCameraManager, dwSvCheats*, SilentAim internals и т.д.) отсутствуют в стандартном дампере.
        // Требуется ручная проверка после обновления на билд 14169.
        inline constexpr std::uintptr_t dwCameraManager = 0x209E570;  // off_18209E570 (asm 14168) — UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetModelName = 0x10BD860;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSetModelString = 0xC19940;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnReloadSubclass = 0x1FAE80;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSetPaintKit = 0x8DEE60;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetRenderItemView = 0x8BB840;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnApplyModel = 0xC1B280;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetPaintKitId = 0x105F9B0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwButterflySubclass = 0x22AB2F0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwSubclassManager = 0x21D9720;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwSvCheatsRef = 0x239D5F0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwSvCheatsConVar = 0x239D5F8;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwSvCheatsRef2 = 0x239D5F0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnResolveConVarValue = 0x18607C0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwForceJump = 0x2093490;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSetColorModulation = 0xB91580;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnReapplyTint = 0x8E4150;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSetModel = 0x920630;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSetGraphDefinition = 0x8CEFE0;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetAgentId = 0x810730;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnAgentModelResolve = 0x7218E0;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnRecreateGraphInstance = 0x8AA770;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnApplyModelHandle = 0x9206D0;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwGameModelInfo = 0x239E7D8;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnLiveSetModel = 0xC19930;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnLiveApply = 0xC518F0;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetLocalPawnLive = 0x8E3970;   // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwFileSystem = 0x239E7D0;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t dwResourceSystem = 0x25C7410;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnBuildResourceName = 0x17F4B90;  // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnCheckResourceType = 0x17F42A0;  // UPDATE REQUIRED

        // Дополнительные глобалы из дампера 14169 (sezzyaep/CS2-OFFSETS)
        // inline constexpr std::uintptr_t dwGameRules          = 0x1A515A8; // пример
        // inline constexpr std::uintptr_t dwGlobalVars         = 0x208FD60;
    }

    namespace Schema
    {
        // === Всё из schemas.hpp (билд 14169, sezzyaep/CS2-OFFSETS) ===
        // Большинство member offsets не изменились между 14168 и 14169.
        inline constexpr std::ptrdiff_t m_iHealth = 0x34C;
        inline constexpr std::ptrdiff_t m_iIDEntIndex = 0x341C;
        inline constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;
        inline constexpr std::ptrdiff_t m_pCameraServices = 0x1240;
        inline constexpr std::ptrdiff_t m_hPlayerPawn = 0x914;
        inline constexpr std::ptrdiff_t m_hPawn = 0x6BC;
        inline constexpr std::ptrdiff_t m_hController = 0x13D0;
        inline constexpr std::ptrdiff_t m_steamID = 0x780;
        inline constexpr std::ptrdiff_t m_bPawnIsAlive = 0x91C;

        inline constexpr std::ptrdiff_t m_pObserverServices = 0x1220;
        inline constexpr std::ptrdiff_t obs_iObserverMode = 0x48;
        inline constexpr std::ptrdiff_t obs_hObserverTarget = 0x4C;

        inline constexpr std::ptrdiff_t m_pWeaponServices = 0x1208;
        inline constexpr std::ptrdiff_t m_hMyWeapons = 0x48;
        inline constexpr std::ptrdiff_t m_hActiveWeapon = 0x60;
        inline constexpr std::ptrdiff_t m_hOwnerEntity = 0x520;
        inline constexpr std::ptrdiff_t m_AttributeManager = 0x11A8; // C_EconEntity
        inline constexpr std::ptrdiff_t m_Item = 0x50;
        inline constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
        inline constexpr std::ptrdiff_t m_iEntityQuality = 0x1BC;
        inline constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0;
        inline constexpr std::ptrdiff_t m_iItemIDLow = 0x1D4;
        inline constexpr std::ptrdiff_t m_iAccountID = 0x1D8;
        inline constexpr std::ptrdiff_t m_bInitialized = 0x1E8;
        inline constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x1680;
        inline constexpr std::ptrdiff_t m_nFallbackSeed = 0x1684;
        inline constexpr std::ptrdiff_t m_flFallbackWear = 0x1688;
        inline constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x168C;
        inline constexpr std::ptrdiff_t m_nSubclassID = 0x380;
        inline constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
        inline constexpr std::ptrdiff_t m_pRenderComponent = 0x338;
        inline constexpr std::ptrdiff_t m_CBodyComponent = 0x30;
        inline constexpr std::ptrdiff_t m_clrRender = 0xC98;
        inline constexpr std::ptrdiff_t m_ClientOverrideTint = 0xF60;
        inline constexpr std::ptrdiff_t m_bUseClientOverrideTint = 0xF64;
        inline constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
        inline constexpr std::ptrdiff_t m_vecViewOffset = 0xE78;
        inline constexpr std::ptrdiff_t m_bVisualsDataSet = 0x18E1;
        inline constexpr std::ptrdiff_t m_fFlags = 0x3F4;
        inline constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3F8;
        inline constexpr std::ptrdiff_t m_vecVelocity = 0x430;
        inline constexpr std::ptrdiff_t m_entitySpottedState = 0x1C58;
        inline constexpr std::ptrdiff_t m_bSpotted = 0x8;
        inline constexpr std::ptrdiff_t m_bSpottedByMask = 0xC;
        inline constexpr std::ptrdiff_t m_pMovementServices = 0x1248; // pawn -> services
        inline constexpr std::ptrdiff_t ms_flMaxspeed = 0x1AC;
        inline constexpr std::ptrdiff_t ms_flForwardMove = 0x1C0;
        inline constexpr std::ptrdiff_t ms_flLeftMove = 0x1C4;

        // === НЕ В ДАМПЕРЕ — пересверить (билд 14169) ===
        inline constexpr std::ptrdiff_t m_szModelNameLive = 0x34B0;  // asm 14168 [entity+0x34B0]
        inline constexpr std::ptrdiff_t m_bModelNameDirty = 0x34EC;  // asm 14168
        inline constexpr std::ptrdiff_t m_bModelDirty = 0x34EC;
        inline constexpr std::ptrdiff_t m_nAgentId = 0x922;
        inline constexpr std::ptrdiff_t m_pSubclassVData = 0x388;
    }

    // Остальные namespaces (EconView, Subclass, FileSystem, AnimGraph, ResourceLoad, CustomModel,
    // EntityList, CustomSkins, JumpBoost, AimMode, NightMode, ThirdpersonCam, CameraView, SilentAim,
    // PacketGuard, EngineInput, Weapon, PawnCombat, AimFn) оставлены без изменений.
    // SilentAim::fnCreateMove, AimFn::* и большинство внутренних структур требуют повторной проверки
    // через реверс (MyGame.asm / x64dbg / pattern scan) на билде 14169.

    namespace EconView
    {
        inline constexpr std::size_t    kPaintGetterIndex = 13;
        inline constexpr std::size_t    kVisualsGetterIndex = 12;
        inline constexpr std::ptrdiff_t kCompositePaintData = 0x1F0;
    }

    namespace Subclass
    {
        inline constexpr std::ptrdiff_t kFlags = 0x6C;
        inline constexpr std::ptrdiff_t kEntries = 0x70;
        inline constexpr std::ptrdiff_t kCount = 0x78;
        inline constexpr std::ptrdiff_t kCapacity = 0x7C;
        inline constexpr std::ptrdiff_t kSlotStride = 0x18;
        inline constexpr std::ptrdiff_t kSlotKey = 0x08;
        inline constexpr std::ptrdiff_t kSlotVData = 0x10;
        inline constexpr std::ptrdiff_t kVDataCategory = 0x08;
        inline constexpr std::ptrdiff_t kVDataToken = 0x0C;
        inline constexpr std::ptrdiff_t kVDataName = 0x10;
    }

    namespace FileSystem
    {
        inline constexpr std::size_t    kAddSearchPathIndex = 0x190 / 8;
        inline constexpr std::ptrdiff_t m_modelState = 0x150;
        inline constexpr std::ptrdiff_t m_nModelID = 0x8;
    }

    namespace AnimGraph
    {
        inline constexpr std::ptrdiff_t m_animationController = 0x4E0;
        inline constexpr std::ptrdiff_t m_hGraphDefinitionAG2 = 0x370;
        inline constexpr std::ptrdiff_t m_pGraphInstanceAG2 = 0x448;
        inline constexpr std::ptrdiff_t kDirtyFlag = 0x688;
        inline constexpr std::ptrdiff_t m_pControllerOnSkeleton = 0x3D0;
        inline constexpr std::ptrdiff_t m_pSkeletonOnController = 0x08;
        inline constexpr std::uint64_t  kTagVnmGraph = 0x68706172676D6E76;
    }

    namespace ResourceLoad
    {
        inline constexpr std::size_t   kLoadIndex = 0x140 / 8;
        inline constexpr std::size_t   kCheckIndex = 0x198 / 8;
        inline constexpr std::size_t   kGetBindingIndex = 0x278 / 8;
        inline constexpr std::uint64_t kTypeVmdl = 0x6C646D76;
    }

    namespace CustomModel
    {
        inline constexpr const char* kPathID = "GAME";

        inline constexpr const char* kModelAositala = "characters/models/kolka/2026/aositala/aositala.vmdl";
        inline constexpr const char* kModelYidhari = "characters/models/kolka/2026/yidhari/yidhari.vmdl";

        inline constexpr const char* kModelPath = kModelYidhari;
        inline constexpr const char* kModelToken = "yidhari";
        inline constexpr const char* kContentRoot = "D:\\Nemesis\\NemesisDLC\\CUSTOM RENDER\\Custom render Model\\model3";
    }

    namespace EntityList
    {
        inline constexpr std::uintptr_t kChunkStep = 0x8;
        inline constexpr std::uintptr_t kChunkBase = 0x10;
        inline constexpr std::uintptr_t kEntryStride = 0x70;
        inline constexpr std::uint32_t  kIndexMask = 0x7FFF;
        inline constexpr std::uint32_t  kChunkShift = 9;
        inline constexpr std::uint32_t  kSlotMask = 0x1FF;
    }

    namespace CustomSkins
    {
        inline constexpr std::uint16_t kKnifeDefIndex = 515;
        inline constexpr int           kPaintKit = 415;
        inline constexpr int           kSeed = 0;
        inline constexpr float         kWear = 0.0f;
        inline constexpr const char* kSubclassName = "weapon_knife_butterfly";
        inline constexpr std::uint32_t kTokenSeed = 0x31415926;
    }

    namespace JumpBoost
    {
        inline constexpr int           kHoldKey = 0x5A;   // Z
        inline constexpr std::uint32_t kOnGroundFlag = 0x1;    // FL_ONGROUND
        inline constexpr std::uint32_t kPress = 65537;  // dwForceJump: прыжок зажат
        inline constexpr std::uint32_t kRelease = 256;    // dwForceJump: отпущен
        inline constexpr int           kPollMs = 1;
        inline constexpr int           kJumpDelayMs = 0;      // мгновенный реджамп (release уже есть в воздухе)
        inline constexpr std::ptrdiff_t kViewYawOff = 0x4;    // yaw в QAngle dwViewAngles
        inline constexpr float         kStrafeMove = 10000.0f; // сила бок. ввода (движок клампит к maxspeed)
        inline constexpr float         kYawDeadzone = 0.01f;  // мертвая зона поворота мыши — меньше = резче
    }

    namespace AimMode
    {
        inline constexpr int kToggleKey = 0x79;   // F10 — переключить рендер/аим Rage<->Legit
        inline constexpr int kStartRage = 0;      // старт: 0 = Legit, 1 = Rage
    }

    namespace NightMode
    {
        inline constexpr int      kToggleKey = 0x78;   // F9
        inline constexpr unsigned kTintR = 6;
        inline constexpr unsigned kTintG = 12;
        inline constexpr unsigned kTintB = 40;
        inline constexpr unsigned kTintA = 140;
    }

    namespace ThirdpersonCam
    {
        inline constexpr std::ptrdiff_t kEnableFlag = 0x229;
        inline constexpr std::ptrdiff_t kAnglePitch = 0x230;
        inline constexpr std::ptrdiff_t kAngleYaw = 0x234;
        inline constexpr std::ptrdiff_t kDistance = 0x238;
    }

    namespace CameraView
    {
        inline constexpr int          kToggleKey = 0x58;
        inline constexpr std::uint32_t kToggleScan = 0x2D;
        inline constexpr float        kDistance = 120.0f;
    }

    namespace SilentAim
    {
        inline constexpr std::uintptr_t fnCreateMove = 0xC97330; // CCSGOInput::CreateMove (asm 14168) — UPDATE REQUIRED for 14169
        inline constexpr std::ptrdiff_t kCmdCount = 0xBC8;    // int — число команд
        inline constexpr std::ptrdiff_t kCmdData = 0xBD0;    // ptr — база вектора команд
        inline constexpr std::ptrdiff_t kCmdStride = 0x60;     // РЕАЛЬНЫЙ страйд команды
        inline constexpr std::ptrdiff_t kViewAngle = 0x10;     // viewangles QAngle в команде
        inline constexpr std::ptrdiff_t kCmdShootPos = 0x1C;   // shoot_position (не трогаем)
        inline constexpr std::ptrdiff_t kCmdTargetHead = 0x28;   // target_head_position_check
        inline constexpr std::ptrdiff_t kCmdTargetAbs = 0x34;   // target_abs_position_check
        inline constexpr std::ptrdiff_t kBaseAngleA = 0x2A0;  // eye/base viewangles (sub_180225A10)
        inline constexpr std::ptrdiff_t kBaseAngleB = 0x758;  // вторая копия текущего угла
        inline constexpr std::ptrdiff_t kCameraAngle = 0x688;  // камера — НЕ трогать
        inline constexpr std::ptrdiff_t kBasePtrA = 0xB58;  // self+0xB58 -> (+0x48) QAngle
        inline constexpr std::ptrdiff_t kBasePtrAInner = 0x48;
        inline constexpr std::ptrdiff_t kBasePtrB = 0xFF8;  // self+0xFF8 -> (+0x27C) QAngle
        inline constexpr std::ptrdiff_t kBasePtrBInner = 0x27C;
        inline constexpr std::ptrdiff_t kAnglePitch = 0x0;    // QAngle: pitch
        inline constexpr std::ptrdiff_t kAngleYaw = 0x4;    // QAngle: yaw
        inline constexpr int            kCmdCountMax = 150;    // sanity-лимит числа команд
    }

    namespace PacketGuard
    {
        inline constexpr std::uintptr_t kHeapMin = 0x10000;
        inline constexpr std::uintptr_t kHeapMax = 0x7FFFFFFFFFFFull;
        inline constexpr float          kNonZeroVecSq = 1.0f;

        inline constexpr bool           kWriteBasePtrA = true;
        inline constexpr bool           kWriteBasePtrB = true;

        inline constexpr std::ptrdiff_t kDiagSelfSpan = 0x1200;
        inline constexpr std::ptrdiff_t kDiagP1Span = 0x400;
        inline constexpr std::ptrdiff_t kDiagP2PtrSpan = 0x180;
        inline constexpr std::ptrdiff_t kDiagP2Span = 0x40;
        inline constexpr float          kDiagAngTol = 0.3f;
        inline constexpr float          kDiagAngMin = 0.05f;
        inline constexpr unsigned       kDiagInlineMs = 400;
        inline constexpr unsigned       kDiagPtrMs = 700;
        inline constexpr int            kDiagInlineHits = 24;
        inline constexpr int            kDiagPtrHits = 40;
    }

    namespace EngineInput
    {
        inline constexpr std::uintptr_t dwFrameCounter = 0x90B688;
        inline constexpr std::uintptr_t dwFrameRing = 0x90C2B0;
        inline constexpr std::ptrdiff_t kRecStride = 0x28;
        inline constexpr std::ptrdiff_t kRecPtr = 0x18;
        inline constexpr int            kRingSize = 10;
    }

    namespace Weapon
    {
        inline constexpr std::ptrdiff_t m_pVData = 0x388;
        inline constexpr std::ptrdiff_t m_nFireMode = 0x17B8;
        inline constexpr std::ptrdiff_t m_fAccuracyPenalty = 0x17F0;
        inline constexpr std::ptrdiff_t m_iRecoilIndex = 0x17FC;
        inline constexpr std::ptrdiff_t m_flRecoilIndex = 0x1800;
        inline constexpr std::ptrdiff_t m_flNextClientFire = 0x1930;
        inline constexpr std::ptrdiff_t vd_flSpread = 0x758;
        inline constexpr std::ptrdiff_t vd_flInaccuracyCrouch = 0x760;
        inline constexpr std::ptrdiff_t vd_flInaccuracyStand = 0x768;
        inline constexpr std::ptrdiff_t vd_flInaccuracyJump = 0x770;
        inline constexpr std::ptrdiff_t vd_flInaccuracyFire = 0x788;
        inline constexpr std::ptrdiff_t vd_flInaccuracyMove = 0x790;
        inline constexpr std::ptrdiff_t vd_nNumBullets = 0x738;
    }

    namespace PawnCombat
    {
        inline constexpr std::ptrdiff_t m_iShotsFired = 0x1C84;
        inline constexpr std::ptrdiff_t m_bIsScoped = 0x1C70;
        inline constexpr std::ptrdiff_t m_zoomLevel = 0x1CE0;
        inline constexpr std::ptrdiff_t m_bFireBulletsSeedSynchronized = 0x95D;
        inline constexpr std::ptrdiff_t m_pAimPunchServices = 0x14B8;
        inline constexpr std::ptrdiff_t ap_predictableBaseAngle = 0x50;
        inline constexpr std::ptrdiff_t ap_predictableBaseAngleVel = 0x5C;
    }

    namespace AimFn
    {
        inline constexpr std::uintptr_t fnFireBullets = 0xC81E30; // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSpreadGen = 0xC826A0; // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnSeedSha1 = 0xC81D80; // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnAngleQuantize = 0xC7BEB0; // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnGetAimPunchAngle = 0x7DB260; // UPDATE REQUIRED
        inline constexpr std::uintptr_t fnWeaponFire = 0x78F9D0; // UPDATE REQUIRED
        inline constexpr float          kRecoilScale = 2.0f;
        inline constexpr std::uint32_t  kFlOnGround = 0x1;
        inline constexpr float          kAngleGrid = 0.5f;
    }
}