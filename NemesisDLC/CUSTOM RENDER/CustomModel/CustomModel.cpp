#include "CustomModel.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"
#include "../../ConfigSavingSystem/FriendModels.hpp"
#include "generated/embedded_models.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>

#include "MinHook.h"

namespace Nemesis::CustomModel
{
    using namespace Nemesis::Addresses;

    namespace
    {
        struct ResourceName
        {
            std::uint32_t length;
            std::uint32_t allocated;
            char          inlineName[0xC8];
            std::uint64_t unknownD0;
            std::uint64_t type;
        };

        using AddSearchPathFn     = bool(__fastcall*)(void*, const char*, const char*);
        using BuildResourceNameFn = void(__fastcall*)(ResourceName*, const char*);
        using CheckResourceTypeFn = bool(__fastcall*)(ResourceName*, std::uint64_t);
        using ResourceLoadFn      = void*(__fastcall*)(void*, ResourceName*, const char*);
        using ResourceCheckFn     = int(__fastcall*)(void*, ResourceName*);
        using ResourceBindingFn   = void*(__fastcall*)(void*, ResourceName*, int);
        using SetModelFn          = void(__fastcall*)(void*, const char*);
        using GetLocalPawnFn      = void*(__fastcall*)(int);
        using LiveApplyFn         = void(__fastcall*)(void*);

        LiveApplyFn    g_origLiveApply = nullptr;
        bool           g_installed = false;
        bool           g_deployed = false;
        bool           g_mounted = false;
        std::uintptr_t g_livePawn = 0;
        std::uintptr_t g_rawApplied = 0;

        bool HeapPtr(std::uintptr_t value)
        {
            return value >= 0x10000 && value <= 0x7FFFFFFFFFFFull;
        }

        std::uintptr_t EntityFromHandle(std::uintptr_t base, std::uint32_t handle)
        {
            if (handle == 0xFFFFFFFF)
                return 0;
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!HeapPtr(list))
                return 0;
            const std::uint32_t index = handle & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (index >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!HeapPtr(chunk))
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (index & EntityList::kSlotMask));
        }

        bool IsLocalPawn(std::uintptr_t base, std::uintptr_t entity)
        {
            if (!HeapPtr(entity))
                return false;
            if (entity == g_livePawn)
                return true;
            const std::uintptr_t localController = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
            if (!HeapPtr(localController))
                return false;
            const std::uint32_t ctrlHandle = Mem::Read<std::uint32_t>(entity + Schema::m_hController, 0xFFFFFFFF);
            return EntityFromHandle(base, ctrlHandle) == localController;
        }

        bool CurrentModelIs(std::uintptr_t pawn, const char* token)
        {
            const std::uintptr_t namePtr = Mem::Read<std::uintptr_t>(pawn + Schema::m_szModelNameLive);
            if (!HeapPtr(namePtr) || !token)
                return false;
            __try { return std::strstr(reinterpret_cast<const char*>(namePtr), token) != nullptr; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        std::uintptr_t GetLivePawn(std::uintptr_t base)
        {
            __try
            {
                const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
                return HeapPtr(pawn) ? pawn : 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        void EnsureMounted(std::uintptr_t base)
        {
            if (g_mounted)
                return;
            const std::uintptr_t fs = Mem::Read<std::uintptr_t>(base + Client::dwFileSystem);
            const std::uintptr_t vt = fs ? Mem::Read<std::uintptr_t>(fs) : 0;
            const std::uintptr_t fn = vt ? Mem::Read<std::uintptr_t>(vt + FileSystem::kAddSearchPathIndex * 8) : 0;
            if (!HeapPtr(fn))
                return;
            __try
            {
                reinterpret_cast<AddSearchPathFn>(fn)(reinterpret_cast<void*>(fs),
                    Addresses::CustomModel::kContentRoot, Addresses::CustomModel::kPathID);
                g_mounted = true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        bool EnsureLoaded(std::uintptr_t base, const char* modelPath)
        {
            const std::uintptr_t rs = Mem::Read<std::uintptr_t>(base + Client::dwResourceSystem);
            const std::uintptr_t vt = rs ? Mem::Read<std::uintptr_t>(rs) : 0;
            const std::uintptr_t loadFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kLoadIndex * 8) : 0;
            const std::uintptr_t checkFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kCheckIndex * 8) : 0;
            const std::uintptr_t bindFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kGetBindingIndex * 8) : 0;
            if (!HeapPtr(loadFn) || !HeapPtr(checkFn) || !HeapPtr(bindFn))
                return false;
            void* binding = nullptr;
            __try
            {
                ResourceName name;
                std::memset(&name, 0, sizeof(name));
                name.allocated = 0xC00000C8;
                reinterpret_cast<BuildResourceNameFn>(base + Client::fnBuildResourceName)(&name, modelPath);
                reinterpret_cast<CheckResourceTypeFn>(base + Client::fnCheckResourceType)(&name, ResourceLoad::kTypeVmdl);
                reinterpret_cast<ResourceLoadFn>(loadFn)(reinterpret_cast<void*>(rs), &name, "NemesisCustomModel");
                if (reinterpret_cast<ResourceCheckFn>(checkFn)(reinterpret_cast<void*>(rs), &name))
                    binding = reinterpret_cast<ResourceBindingFn>(bindFn)(reinterpret_cast<void*>(rs), &name, 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
            return binding != nullptr;
        }

        std::uint64_t PawnSteamID(std::uintptr_t base, std::uintptr_t pawn)
        {
            const std::uint32_t ctrlHandle = Mem::Read<std::uint32_t>(pawn + Schema::m_hController, 0xFFFFFFFF);
            const std::uintptr_t controller = EntityFromHandle(base, ctrlHandle);
            if (!HeapPtr(controller))
                return 0;
            return Mem::Read<std::uint64_t>(controller + Schema::m_steamID, 0);
        }

        void ApplyModelInHook(std::uintptr_t base, std::uintptr_t pawn, const char* model, const char* token)
        {
            if (!model || CurrentModelIs(pawn, token) || !EnsureLoaded(base, model))
                return;
            Mem::Write<std::uintptr_t>(pawn + Schema::m_szModelNameLive, reinterpret_cast<std::uintptr_t>(model));
            Mem::Write<std::uint8_t>(pawn + Schema::m_bModelNameDirty, 1);
        }

        void __fastcall hkLiveApply(void* entity)
        {
            const std::uintptr_t base = entity ? Mem::ModuleBase(Modules::kClient) : 0;
            if (Enabled && base)
            {
                const std::uintptr_t pawn = reinterpret_cast<std::uintptr_t>(entity);
                if (IsLocalPawn(base, pawn))
                {
                    ApplyModelInHook(base, pawn, Addresses::CustomModel::kModelPath, Addresses::CustomModel::kModelToken);
                }
                else if (Nemesis::FriendModels::LoadedCount() > 0)
                {
                    const std::uint64_t steamID = PawnSteamID(base, pawn);
                    const char* token = nullptr;
                    const char* model = Nemesis::FriendModels::MatchSteamID(steamID, token);
                    static int s_flog = 0;
                    if (s_flog < 40)
                    {
                        ++s_flog;
                        NLOG("[friend] hook non-local pawn=%p steamID=%llu matched=%d",
                             entity, static_cast<unsigned long long>(steamID), model ? 1 : 0);
                    }
                    ApplyModelInHook(base, pawn, model, token);
                }
            }
            g_origLiveApply(entity);
        }

        void FallbackRawSetModel(std::uintptr_t base, std::uintptr_t pawn)
        {
            if (pawn == g_rawApplied || CurrentModelIs(pawn, Addresses::CustomModel::kModelToken))
                return;
            if (Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
                return;
            if (!EnsureLoaded(base, Addresses::CustomModel::kModelPath))
                return;
            g_rawApplied = pawn;
            __try
            {
                reinterpret_cast<SetModelFn>(base + Client::fnSetModel)(
                    reinterpret_cast<void*>(pawn), Addresses::CustomModel::kModelPath);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        void InstallHook(std::uintptr_t base)
        {
            if (g_installed)
                return;
            g_installed = true;
            MH_Initialize();
            void* target = reinterpret_cast<void*>(base + Client::fnLiveApply);
            if (MH_CreateHook(target, &hkLiveApply, reinterpret_cast<void**>(&g_origLiveApply)) == MH_OK &&
                MH_EnableHook(target) == MH_OK)
                NLOG("[custommodel] liveapply hook installed @%p", target);
            else
                NERR("[custommodel] liveapply hook failed");
        }
    }

    void Render()
    {
        if (!Enabled)
            return;

        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base)
            return;

        if (!g_deployed)
        {
            g_deployed = true;
            EmbeddedModels::Deploy();
        }

        InstallHook(base);
        EnsureMounted(base);
        g_livePawn = GetLivePawn(base);

        if (HeapPtr(g_livePawn))
        {
            static std::uintptr_t s_lastPawn = 0;
            static DWORD s_seen = 0;
            static std::uint8_t s_lastTeam = 0;

            if (g_livePawn != s_lastPawn) { s_lastPawn = g_livePawn; s_seen = GetTickCount(); }

            const std::uint8_t team = Mem::Read<std::uint8_t>(g_livePawn + Schema::m_iTeamNum, 0);
            if (team != s_lastTeam) { s_lastTeam = team; g_rawApplied = 0; s_seen = GetTickCount(); }

            if (GetTickCount() - s_seen > 3000)
                FallbackRawSetModel(base, g_livePawn);
        }
    }
}
