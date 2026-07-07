#include "CustomModel.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"
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
        bool           g_mounted = false;
        void*          g_binding = nullptr;
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

        bool IsGfl2(std::uintptr_t pawn)
        {
            const std::uintptr_t namePtr = Mem::Read<std::uintptr_t>(pawn + Schema::m_szModelNameLive);
            if (!HeapPtr(namePtr))
                return false;
            __try
            {
                return std::strstr(reinterpret_cast<const char*>(namePtr), Addresses::CustomModel::kModelToken) != nullptr;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        std::uintptr_t GetLivePawn(std::uintptr_t base)
        {
            __try
            {
                const std::uintptr_t pawn = reinterpret_cast<std::uintptr_t>(
                    reinterpret_cast<GetLocalPawnFn>(base + Client::fnGetLocalPawnLive)(-1));
                return HeapPtr(pawn) ? pawn : 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        bool EnsureMounted(std::uintptr_t base)
        {
            if (g_mounted)
                return true;
            const std::uintptr_t fs = Mem::Read<std::uintptr_t>(base + Client::dwFileSystem);
            const std::uintptr_t vt = fs ? Mem::Read<std::uintptr_t>(fs) : 0;
            const std::uintptr_t fn = vt ? Mem::Read<std::uintptr_t>(vt + FileSystem::kAddSearchPathIndex * 8) : 0;
            if (!HeapPtr(fn))
                return false;
            __try
            {
                reinterpret_cast<AddSearchPathFn>(fn)(reinterpret_cast<void*>(fs),
                    Addresses::CustomModel::kContentRoot, Addresses::CustomModel::kPathID);
                g_mounted = true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
            return g_mounted;
        }

        bool EnsureLoaded(std::uintptr_t base)
        {
            if (g_binding)
                return true;
            if (!EnsureMounted(base))
                return false;

            const std::uintptr_t rs = Mem::Read<std::uintptr_t>(base + Client::dwResourceSystem);
            const std::uintptr_t vt = rs ? Mem::Read<std::uintptr_t>(rs) : 0;
            const std::uintptr_t loadFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kLoadIndex * 8) : 0;
            const std::uintptr_t checkFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kCheckIndex * 8) : 0;
            const std::uintptr_t bindFn = vt ? Mem::Read<std::uintptr_t>(vt + ResourceLoad::kGetBindingIndex * 8) : 0;
            if (!HeapPtr(loadFn) || !HeapPtr(checkFn) || !HeapPtr(bindFn))
                return false;

            __try
            {
                ResourceName name;
                std::memset(&name, 0, sizeof(name));
                name.allocated = 0xC00000C8;
                reinterpret_cast<BuildResourceNameFn>(base + Client::fnBuildResourceName)(&name, Addresses::CustomModel::kModelPath);
                reinterpret_cast<CheckResourceTypeFn>(base + Client::fnCheckResourceType)(&name, ResourceLoad::kTypeVmdl);
                reinterpret_cast<ResourceLoadFn>(loadFn)(reinterpret_cast<void*>(rs), &name, "NemesisCustomModel");
                if (reinterpret_cast<ResourceCheckFn>(checkFn)(reinterpret_cast<void*>(rs), &name))
                    g_binding = reinterpret_cast<ResourceBindingFn>(bindFn)(reinterpret_cast<void*>(rs), &name, 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
            return g_binding != nullptr;
        }

        void __fastcall hkLiveApply(void* entity)
        {
            const std::uintptr_t base = entity ? Mem::ModuleBase(Modules::kClient) : 0;
            if (Enabled && base && IsLocalPawn(base, reinterpret_cast<std::uintptr_t>(entity)))
            {
                const std::uintptr_t pawn = reinterpret_cast<std::uintptr_t>(entity);
                if (EnsureLoaded(base) && !IsGfl2(pawn))
                {
                    Mem::Write<std::uintptr_t>(pawn + Schema::m_szModelNameLive,
                        reinterpret_cast<std::uintptr_t>(Addresses::CustomModel::kModelPath));
                    Mem::Write<std::uint8_t>(pawn + Schema::m_bModelNameDirty, 1);
                    NLOG("[custommodel] liveapply hook -> gfl2 pawn=%p", entity);
                }
            }
            g_origLiveApply(entity);
        }

        void FallbackRawSetModel(std::uintptr_t base, std::uintptr_t pawn)
        {
            if (pawn == g_rawApplied || IsGfl2(pawn))
                return;
            if (Mem::Read<int>(pawn + Schema::m_iHealth, 0) <= 0)
                return;
            if (!EnsureLoaded(base))
                return;
            g_rawApplied = pawn;
            __try
            {
                reinterpret_cast<SetModelFn>(base + Client::fnSetModel)(
                    reinterpret_cast<void*>(pawn), Addresses::CustomModel::kModelPath);
                NLOG("[custommodel] fallback raw setmodel pawn=%p", reinterpret_cast<void*>(pawn));
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { NERR("[custommodel] fallback crashed pawn=%p", reinterpret_cast<void*>(pawn)); }
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

        static bool s_deployed = false;
        if (!s_deployed)
        {
            s_deployed = true;
            EmbeddedModels::Deploy();
        }

        InstallHook(base);
        EnsureMounted(base);
        g_livePawn = GetLivePawn(base);

        static DWORD s_seen = 0;
        if (HeapPtr(g_livePawn))
        {
            static std::uintptr_t s_lastPawn = 0;
            if (g_livePawn != s_lastPawn) { s_lastPawn = g_livePawn; s_seen = GetTickCount(); }
            if (GetTickCount() - s_seen > 3000)
                FallbackRawSetModel(base, g_livePawn);
        }
    }
}
