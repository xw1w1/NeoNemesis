#include "CustomSkins.hpp"
#include "../CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <Windows.h>

namespace Nemesis::CustomSkins
{
    using namespace Nemesis::Addresses;

    namespace
    {
        using GetModelNameFn    = const char* (__fastcall*)(void* itemView);
        using SetModelStringFn  = void (__fastcall*)(void* entity, const char* model);
        using ReloadSubclassFn  = void (__fastcall*)(void* entity);
        using SetPaintKitFn     = void (__fastcall*)(void* entity, int paintKit, int a3, int a4);
        using GetRenderViewFn   = void* (__fastcall*)(void* entity);

        std::atomic<bool> g_running{ false };
        std::thread       g_worker;

        const char* SafeGetModel(GetModelNameFn fn, void* itemView)
        {
            __try { return fn(itemView); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }

        bool SafeSetModel(SetModelStringFn fn, void* entity, const char* model)
        {
            __try { fn(entity, model); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool SafeReloadSubclass(ReloadSubclassFn fn, void* entity)
        {
            __try { fn(entity); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool SafeApplyModel(ReloadSubclassFn fn, void* entity)
        {
            __try { fn(entity); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool SafeSetPaintKit(SetPaintKitFn fn, void* entity, int paintKit)
        {
            __try { fn(entity, paintKit, 1, 0); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        int SafeReadAppliedPaint(GetRenderViewFn fn, void* entity)
        {
            __try
            {
                void* view = fn(entity);
                if (!view)
                    return -3;
                void** vtable = *reinterpret_cast<void***>(view);
                auto getPaint = reinterpret_cast<void (__fastcall*)(void*, int*)>(vtable[EconView::kPaintGetterIndex]);
                int out = -1;
                getPaint(view, &out);
                return out;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -2;
            }
        }

        std::uintptr_t EntityFromHandle(std::uintptr_t clientBase, std::uint32_t handle)
        {
            if (handle == 0xFFFFFFFF)
                return 0;

            const std::uintptr_t list = Mem::Read<std::uintptr_t>(clientBase + Client::dwEntityList);
            if (!list)
                return 0;

            const std::uint32_t index = handle & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (index >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;

            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (index & EntityList::kSlotMask));
        }

        std::uint32_t MakeToken(const char* s)
        {
            const std::uint32_t m = 0x5BD1E995u;
            auto lower = [](unsigned char c) -> std::uint32_t
            {
                return (c >= 'A' && c <= 'Z') ? static_cast<std::uint32_t>(c + 0x20) : c;
            };
            std::uint32_t len = 0;
            while (s[len]) ++len;
            const auto* d = reinterpret_cast<const unsigned char*>(s);
            std::uint32_t h = Addresses::CustomSkins::kTokenSeed ^ len;
            std::uint32_t i = 0;
            std::uint32_t l = len;
            while (l >= 4)
            {
                std::uint32_t k = lower(d[i]) | (lower(d[i + 1]) << 8) | (lower(d[i + 2]) << 16) | (lower(d[i + 3]) << 24);
                k *= m; k ^= k >> 24; k *= m;
                h *= m; h ^= k;
                i += 4; l -= 4;
            }
            if (l == 3) h ^= lower(d[i + 2]) << 16;
            if (l >= 2) h ^= lower(d[i + 1]) << 8;
            if (l >= 1) { h ^= lower(d[i]); h *= m; }
            h ^= h >> 13; h *= m; h ^= h >> 15;
            return h;
        }

        void ReadCStr(std::uintptr_t addr, char* out, std::size_t cap)
        {
            std::size_t i = 0;
            for (; addr && i + 1 < cap; ++i)
            {
                const char c = Mem::Read<char>(addr + i);
                if (!c)
                    break;
                out[i] = c;
            }
            out[i] = 0;
        }

        void DumpSubclassRegistry(std::uintptr_t base)
        {
            const std::uintptr_t mgr = Mem::Read<std::uintptr_t>(base + Client::dwSubclassManager);
            if (!mgr)
            {
                NWARN("Subclass manager null");
                return;
            }

            const std::uint32_t  flags   = Mem::Read<std::uint32_t>(mgr + Subclass::kFlags);
            const std::uint32_t  count   = Mem::Read<std::uint32_t>(mgr + Subclass::kCount);
            const std::uint32_t  cap     = Mem::Read<std::uint32_t>(mgr + Subclass::kCapacity);
            const std::uintptr_t entries = Mem::Read<std::uintptr_t>(mgr + Subclass::kEntries);

            NLOG("Subclass registry mgr=0x%p flags=0x%X count=%u cap=%u entries=0x%p",
                 reinterpret_cast<void*>(mgr), flags, count, cap, reinterpret_cast<void*>(entries));

            if (!entries || (flags & 0x7FFFFFFF) == 0 || cap == 0 || cap > 8192)
                return;

            char name[64];
            for (std::uint32_t i = 0; i < cap; ++i)
            {
                const std::uintptr_t slot  = entries + static_cast<std::uintptr_t>(i) * Subclass::kSlotStride;
                const std::uintptr_t vdata = Mem::Read<std::uintptr_t>(slot + Subclass::kSlotVData);
                if (!vdata)
                    continue;

                const std::uint32_t  token   = Mem::Read<std::uint32_t>(vdata + Subclass::kVDataToken);
                const std::uint32_t  cat     = Mem::Read<std::uint32_t>(vdata + Subclass::kVDataCategory);
                const std::uintptr_t nameptr = Mem::Read<std::uintptr_t>(vdata + Subclass::kVDataName);
                if (!nameptr || !token)
                    continue;

                ReadCStr(nameptr, name, sizeof(name));
                if (name[0])
                    NLOG("  [%u] token=0x%08X cat=%d name=%s", i, token, static_cast<int>(cat), name);
            }
        }

        std::uint32_t MakeTokenU(std::uint32_t value)
        {
            char tmp[12];
            char buf[12];
            int t = 0;
            if (value == 0)
                tmp[t++] = '0';
            while (value)
            {
                tmp[t++] = static_cast<char>('0' + value % 10);
                value /= 10;
            }
            int n = 0;
            while (t)
                buf[n++] = tmp[--t];
            buf[n] = 0;
            return MakeToken(buf);
        }

        bool IsKnife(std::uint16_t defIndex)
        {
            if (defIndex == 42 || defIndex == 59)
                return true;
            return defIndex >= 500 && defIndex <= 525;
        }

        std::uint16_t WeaponDef(std::uintptr_t weapon)
        {
            return Mem::Read<std::uint16_t>(weapon + Schema::m_AttributeManager + Schema::m_Item + Schema::m_iItemDefinitionIndex);
        }

        std::uint32_t LocalAccountID(std::uintptr_t base)
        {
            const std::uintptr_t controller = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
            if (!controller)
                return 0;
            return static_cast<std::uint32_t>(Mem::Read<std::uint64_t>(controller + Schema::m_steamID));
        }

        void ApplyToWeapon(std::uintptr_t base, std::uintptr_t weapon)
        {
            const std::uintptr_t item = weapon + Schema::m_AttributeManager + Schema::m_Item;

            if (!IsKnife(WeaponDef(weapon)))
                return;

            static bool subclassLogged = false;
            if (!subclassLogged)
            {
                const std::uint32_t  token = Mem::Read<std::uint32_t>(weapon + Schema::m_nSubclassID);
                const std::uintptr_t vdata = Mem::Read<std::uintptr_t>(weapon + Schema::m_pSubclassVData);
                char name[64];
                name[0] = 0;
                if (vdata)
                    ReadCStr(Mem::Read<std::uintptr_t>(vdata + Subclass::kVDataName), name, sizeof(name));
                NLOG("Equipped knife def=%u subclass token=0x%08X vdata=0x%p name=%s",
                     WeaponDef(weapon), token, reinterpret_cast<void*>(vdata), name);
                subclassLogged = true;
            }

            Mem::Write<std::uint16_t>(item + Schema::m_iItemDefinitionIndex, Addresses::CustomSkins::kKnifeDefIndex);
            Mem::Write<int>(item + Schema::m_iEntityQuality, 3);
            Mem::Write<std::uint32_t>(item + Schema::m_iItemIDHigh, 0xFFFFFFFF);
            Mem::Write<std::uint32_t>(item + Schema::m_iItemIDLow, 0xFFFFFFFF);
            Mem::Write<std::uint32_t>(item + Schema::m_iAccountID, LocalAccountID(base));
            Mem::Write<bool>(item + Schema::m_bInitialized, true);

            Mem::Write<int>(weapon + Schema::m_nFallbackPaintKit, Addresses::CustomSkins::kPaintKit);
            Mem::Write<int>(weapon + Schema::m_nFallbackSeed, Addresses::CustomSkins::kSeed);
            Mem::Write<float>(weapon + Schema::m_flFallbackWear, Addresses::CustomSkins::kWear);
            Mem::Write<int>(weapon + Schema::m_nFallbackStatTrak, -1);

            const auto getModelName = reinterpret_cast<GetModelNameFn>(base + Client::fnGetModelName);
            const auto setModel     = reinterpret_cast<SetModelStringFn>(base + Client::fnSetModelString);

            const char* path = SafeGetModel(getModelName, reinterpret_cast<void*>(item));
            if (path && *path)
                SafeSetModel(setModel, reinterpret_cast<void*>(weapon), path);

            static bool subclassDisabled = false;
            const std::uint32_t want = MakeTokenU(Addresses::CustomSkins::kKnifeDefIndex);
            const std::uint32_t cur  = Mem::Read<std::uint32_t>(weapon + Schema::m_nSubclassID);
            if (!subclassDisabled && cur != want)
            {
                const auto reloadSubclass = reinterpret_cast<ReloadSubclassFn>(base + Client::fnReloadSubclass);
                Mem::Write<std::uint32_t>(weapon + Schema::m_nSubclassID, want);
                SafeReloadSubclass(reloadSubclass, reinterpret_cast<void*>(weapon));

                const std::uintptr_t vd = Mem::Read<std::uintptr_t>(weapon + Schema::m_pSubclassVData);
                if (!vd)
                {
                    Mem::Write<std::uint32_t>(weapon + Schema::m_nSubclassID, cur);
                    SafeReloadSubclass(reloadSubclass, reinterpret_cast<void*>(weapon));
                    subclassDisabled = true;
                    NWARN("Subclass 0x%08X not loaded, reverted to 0x%08X", want, cur);
                }
                else
                    NLOG("Subclass applied 0x%08X vdata=0x%p", want, reinterpret_cast<void*>(vd));
            }

            static int  paintAttempts = 0;
            static bool paintDone = false;
            if (!paintDone && paintAttempts < 40)
            {
                ++paintAttempts;

                const auto setPaintKit   = reinterpret_cast<SetPaintKitFn>(base + Client::fnSetPaintKit);
                const auto getRenderView = reinterpret_cast<GetRenderViewFn>(base + Client::fnGetRenderItemView);
                const auto applyModel    = reinterpret_cast<ReloadSubclassFn>(base + Client::fnApplyModel);

                const bool callOk = SafeSetPaintKit(setPaintKit, reinterpret_cast<void*>(weapon), Addresses::CustomSkins::kPaintKit);

                Mem::Write<std::uint8_t>(weapon + Schema::m_bModelDirty, 1);
                SafeApplyModel(applyModel, reinterpret_cast<void*>(weapon));

                const int applied = SafeReadAppliedPaint(getRenderView, reinterpret_cast<void*>(weapon));

                if (applied == Addresses::CustomSkins::kPaintKit)
                    paintDone = true;

                if (paintAttempts == 1 || paintDone || paintAttempts == 40)
                    NLOG("Paint cfg=%d applied=%d callOk=%d sceneNode=0x%p attempts=%d",
                         Addresses::CustomSkins::kPaintKit, applied, static_cast<int>(callOk),
                         reinterpret_cast<void*>(Mem::Read<std::uintptr_t>(weapon + Schema::m_pGameSceneNode)),
                         paintAttempts);
            }
        }

        void Worker()
        {
            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base)
                {
                    static bool registryDumped = false;
                    if (!registryDumped)
                    {
                        DumpSubclassRegistry(base);
                        registryDumped = true;
                    }

                    const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
                    if (pawn)
                    {
                        const std::uintptr_t ws = Mem::Read<std::uintptr_t>(pawn + Schema::m_pWeaponServices);
                        if (ws)
                        {
                            const int count = Mem::Read<int>(ws + Schema::m_hMyWeapons);
                            const std::uintptr_t data = Mem::Read<std::uintptr_t>(ws + Schema::m_hMyWeapons + 0x8);

                            if (data && count > 0 && count <= 64)
                            {
                                for (int i = 0; i < count; ++i)
                                {
                                    const std::uint32_t handle = Mem::Read<std::uint32_t>(data + 4 * i);
                                    const std::uintptr_t weapon = EntityFromHandle(base, handle);
                                    if (weapon)
                                        ApplyToWeapon(base, weapon);
                                }
                            }
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    void Start()
    {
        if (g_running.exchange(true))
            return;

        g_worker = std::thread(Worker);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        if (g_worker.joinable())
            g_worker.join();
    }
}
