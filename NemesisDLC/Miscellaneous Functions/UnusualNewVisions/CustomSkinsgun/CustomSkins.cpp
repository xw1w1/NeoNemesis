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
        using ReloadSubclassFn = void(__fastcall*)(void* entity);
        using SetModelStringFn = void(__fastcall*)(void* entity, const char* model);

        std::atomic<bool> g_running{ false };
        std::thread       g_worker;

        static const char kButterflyModel[] = "weapons/models/knife/knife_butterfly/weapon_knife_butterfly.vmdl";

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
            const std::uint16_t curDef = Mem::Read<std::uint16_t>(item + Schema::m_iItemDefinitionIndex);
            if (!IsKnife(curDef))
                return;

            const bool fresh = (curDef != Addresses::CustomSkins::kKnifeDefIndex);

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

            if (fresh)
            {
                Mem::Write<std::uint8_t>(weapon + Schema::m_bVisualsDataSet, 0);
                NLOG("[skin] knife def %u -> 515, paint=%d, visuals reset", curDef, Addresses::CustomSkins::kPaintKit);
            }

            const std::uint32_t want = MakeTokenU(Addresses::CustomSkins::kKnifeDefIndex);
            const std::uint32_t cur = Mem::Read<std::uint32_t>(weapon + Schema::m_nSubclassID);
            Mem::Write<std::uint32_t>(weapon + Schema::m_nSubclassID, want);

            const std::uintptr_t sceneNode = Mem::Read<std::uintptr_t>(weapon + Schema::m_pGameSceneNode);
            if (!sceneNode)
                return;

            static bool subclassDisabled = false;
            if (!subclassDisabled && cur != want)
            {
                const auto reloadSubclass = reinterpret_cast<ReloadSubclassFn>(base + Client::fnReloadSubclass);
                SafeReloadSubclass(reloadSubclass, reinterpret_cast<void*>(weapon));

                const std::uintptr_t vd = Mem::Read<std::uintptr_t>(weapon + Schema::m_pSubclassVData);
                if (!vd)
                {
                    Mem::Write<std::uint32_t>(weapon + Schema::m_nSubclassID, cur);
                    SafeReloadSubclass(reloadSubclass, reinterpret_cast<void*>(weapon));
                    subclassDisabled = true;
                    NWARN("[skin] subclass 0x%08X not loaded, reverted to 0x%08X", want, cur);
                }
            }
        }

        std::uintptr_t GetEntityByIndex(std::uintptr_t base, std::uint32_t index)
        {
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list)
                return 0;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (index >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (index & EntityList::kSlotMask));
        }

        void ApplyViewModels(std::uintptr_t base, std::uintptr_t pawn, const std::uintptr_t* weapons, int wc)
        {
            static std::ptrdiff_t s_off = 0;
            static std::ptrdiff_t s_inner = 0;

            if (s_off == 0)
            {
                int logged = 0;
                for (std::ptrdiff_t off = 0x1000; off < 0x2500; off += 8)
                {
                    const std::uintptr_t svc = Mem::Read<std::uintptr_t>(pawn + off);
                    if (svc < 0x10000)
                        continue;
                    const std::uintptr_t vt = Mem::Read<std::uintptr_t>(svc);
                    if (vt < base || vt > base + 0x8000000)
                        continue;

                    for (std::ptrdiff_t in = 0; in <= 0x80; in += 4)
                    {
                        const std::uint32_t h = Mem::Read<std::uint32_t>(svc + in);
                        if (h == 0 || h == 0xFFFFFFFF || (h >> 15) == 0 || (h & 0x7FFF) <= 64)
                            continue;
                        const std::uintptr_t ent = EntityFromHandle(base, h);
                        if (!ent || ent == pawn)
                            continue;
                        if (!Mem::Read<std::uintptr_t>(ent + Schema::m_pGameSceneNode))
                            continue;
                        bool isWeapon = false;
                        for (int i = 0; i < wc; ++i)
                            if (weapons[i] == ent) { isWeapon = true; break; }
                        if (isWeapon)
                            continue;

                        if (logged < 6)
                        {
                            NLOG("[vm] cand off=0x%llX in=0x%llX h=%08X ent=%p",
                                 static_cast<unsigned long long>(off), static_cast<unsigned long long>(in),
                                 h, reinterpret_cast<void*>(ent));
                            ++logged;
                        }
                        if (s_off == 0) { s_off = off; s_inner = in; }
                    }
                }
            }

            if (s_off)
            {
                const std::uintptr_t svc = Mem::Read<std::uintptr_t>(pawn + s_off);
                const std::uintptr_t vm = EntityFromHandle(base, Mem::Read<std::uint32_t>(svc + s_inner));
                if (vm)
                {
                    const auto setModel = reinterpret_cast<SetModelStringFn>(base + Client::fnSetModelString);
                    SafeSetModel(setModel, reinterpret_cast<void*>(vm), kButterflyModel);
                }
            }
        }

        void Worker()
        {
            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base)
                {
                    const std::uintptr_t pawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
                    const int hp = pawn ? Mem::Read<int>(pawn + Schema::m_iHealth, 0) : 0;
                    if (pawn && hp > 0 && hp <= 100)
                    {
                        const std::uintptr_t ws = Mem::Read<std::uintptr_t>(pawn + Schema::m_pWeaponServices);
                        if (ws)
                        {
                            const int count = Mem::Read<int>(ws + Schema::m_hMyWeapons);
                            const std::uintptr_t data = Mem::Read<std::uintptr_t>(ws + Schema::m_hMyWeapons + 0x8);

                            if (data && count > 0 && count <= 64)
                            {
                                std::uintptr_t weapons[64];
                                int wc = 0;
                                for (int i = 0; i < count; ++i)
                                {
                                    const std::uint32_t handle = Mem::Read<std::uint32_t>(data + 4 * i);
                                    const std::uintptr_t weapon = EntityFromHandle(base, handle);
                                    if (weapon)
                                    {
                                        ApplyToWeapon(base, weapon);
                                        weapons[wc++] = weapon;
                                    }
                                }

                                const std::uint32_t activeHandle = Mem::Read<std::uint32_t>(ws + Schema::m_hActiveWeapon);
                                const std::uintptr_t active = EntityFromHandle(base, activeHandle);
                                if (active)
                                {
                                    const std::uint16_t adef = Mem::Read<std::uint16_t>(
                                        active + Schema::m_AttributeManager + Schema::m_Item + Schema::m_iItemDefinitionIndex);
                                    if (adef == Addresses::CustomSkins::kKnifeDefIndex)
                                        ApplyViewModels(base, pawn, weapons, wc);
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