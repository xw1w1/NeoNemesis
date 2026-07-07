#include "CustomModel.hpp"
#include "../../Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "LogsSystem.hpp"

#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace Nemesis::CustomModel
{
    using namespace Nemesis::Addresses;

    namespace
    {
        using AddSearchPathFn = bool(__fastcall*)(void*, const char*, const char*);
        using SetModelFn = void(__fastcall*)(std::uintptr_t, const char*);
        using BuildResNameFn = void(__fastcall*)(void*, const char*);
        using CheckTypeFn = bool(__fastcall*)(void*, std::uint64_t);
        using LoadResourceFn = void(__fastcall*)(void*, void*, const char*);
        using CheckLoadableFn = int(__fastcall*)(void*, void*);
        using GetBindingFn = void*(__fastcall*)(void*, void*, int);

        struct ResName
        {
            std::uint32_t nLength;
            std::uint32_t nAllocated;
            char          data[0xF8];
        };

        bool  g_mounted = false;
        bool  g_held = false;
        void* g_binding = nullptr;
        int   g_setCount = 0;

        bool SafeLoadResource(std::uintptr_t base)
        {
            const std::uintptr_t rs = Mem::Read<std::uintptr_t>(base + Client::dwResourceSystem);
            const std::uintptr_t vt = rs ? Mem::Read<std::uintptr_t>(rs) : 0;
            if (!vt)
            {
                NERR("CustomModel loadres: rs/vt null");
                return false;
            }
            const std::uintptr_t loadFn  = Mem::Read<std::uintptr_t>(vt + ResourceLoad::kLoadIndex * 8);
            const std::uintptr_t checkFn = Mem::Read<std::uintptr_t>(vt + ResourceLoad::kCheckIndex * 8);
            const std::uintptr_t bindFn  = Mem::Read<std::uintptr_t>(vt + ResourceLoad::kGetBindingIndex * 8);

            __try
            {
                ResName rn;
                std::memset(&rn, 0, sizeof(rn));
                rn.nAllocated = 0xC00000C8;

                reinterpret_cast<BuildResNameFn>(base + Client::fnBuildResourceName)(&rn, Addresses::CustomModel::kModelPath);
                const bool okType = reinterpret_cast<CheckTypeFn>(base + Client::fnCheckResourceType)(&rn, ResourceLoad::kTypeVmdl);

                reinterpret_cast<LoadResourceFn>(loadFn)(reinterpret_cast<void*>(rs), &rn, "NemesisCustomModel");

                const int loadable = reinterpret_cast<CheckLoadableFn>(checkFn)(reinterpret_cast<void*>(rs), &rn);
                if (loadable)
                {
                    g_binding = reinterpret_cast<GetBindingFn>(bindFn)(reinterpret_cast<void*>(rs), &rn, 0);
                    g_held = g_binding != nullptr;
                }

                static int s_logged = 0;
                if (s_logged < 6)
                {
                    NLOG("CustomModel loadres: typeVmdl=%d loadable=%d binding=%p", (int)okType, loadable, g_binding);
                    ++s_logged;
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { NERR("CustomModel loadres crashed"); return false; }
        }

        bool SafeAddSearchPath(std::uintptr_t base)
        {
            const std::uintptr_t fs = Mem::Read<std::uintptr_t>(base + Client::dwFileSystem);
            const std::uintptr_t vtbl = fs ? Mem::Read<std::uintptr_t>(fs) : 0;
            const std::uintptr_t fn = vtbl ? Mem::Read<std::uintptr_t>(vtbl + FileSystem::kAddSearchPathIndex * 8) : 0;
            NLOG("CustomModel mount: fs=%p vtbl=%p addSearchPath=%p", (void*)fs, (void*)vtbl, (void*)fn);
            if (!fn)
                return false;

            __try
            {
                const bool ok = reinterpret_cast<AddSearchPathFn>(fn)(
                    reinterpret_cast<void*>(fs), Addresses::CustomModel::kContentRoot, Addresses::CustomModel::kPathID);
                NLOG("CustomModel mount result: %d (root=%s)", (int)ok, Addresses::CustomModel::kContentRoot);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { NERR("CustomModel mount crashed"); return false; }
        }

        void SafeSetModel(std::uintptr_t base, std::uintptr_t pawn)
        {
            const std::uintptr_t fn = base + Client::fnSetModel;
            __try { reinterpret_cast<SetModelFn>(fn)(pawn, Addresses::CustomModel::kModelPath); }
            __except (EXCEPTION_EXECUTE_HANDLER) { NERR("CustomModel SetModel crashed"); }
        }
    }

    void Render()
    {
        if (!Enabled)
            return;

        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base)
            return;

        if (!g_mounted)
        {
            g_mounted = SafeAddSearchPath(base);
            if (!g_mounted)
                return;
        }

        const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
        if (!pawn)
            return;

        const int hp = Mem::Read<int>(pawn + Schema::m_iHealth, 0);
        const bool alive = hp > 0;

        static std::uintptr_t s_lastPawn = 0;
        static bool s_wasAlive = false;
        static int s_cooldown = 0;

        if (pawn != s_lastPawn)
        {
            s_lastPawn = pawn;
            s_cooldown = 0;
            g_held = false;
            g_binding = nullptr;
        }
        if (alive && !s_wasAlive)
        {
            s_cooldown = 0;
            g_held = false;
            g_binding = nullptr;
        }
        s_wasAlive = alive;

        if (!alive)
            return;

        if (!g_held)
            SafeLoadResource(base);

        if (s_cooldown > 0)
        {
            --s_cooldown;
            return;
        }

        SafeSetModel(base, pawn);
        s_cooldown = 120;

        if (g_setCount < 4)
        {
            NLOG("CustomModel SetModel applied: pawn=%p", (void*)pawn);
            ++g_setCount;
        }
    }
}
