#include "InventoryGive.hpp"
#include "../CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <Windows.h>

namespace Nemesis::InventoryGive
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::atomic<bool> g_running{ false };
        std::thread       g_worker;

        constexpr std::ptrdiff_t kInvServices = 0x810;
        constexpr std::ptrdiff_t kLoadoutVec  = 0x40;
        constexpr std::ptrdiff_t kSlotItemPtr = 0x00;

        constexpr std::ptrdiff_t kSlotStrideA = 0x10;
        constexpr std::ptrdiff_t kSlotStrideB = 0x08;

        constexpr std::ptrdiff_t kDefIndex   = 0x1BA;
        constexpr std::ptrdiff_t kQuality    = 0x1BC;
        constexpr std::ptrdiff_t kItemIDHigh = 0x1D0;
        constexpr std::ptrdiff_t kItemIDLow  = 0x1D4;
        constexpr std::ptrdiff_t kAccountID  = 0x1D8;
        constexpr std::ptrdiff_t kInit       = 0x1E8;

        bool IsKnife(std::uint16_t d) { return d == 42 || d == 59 || (d >= 500 && d <= 525); }

        void DumpLoadout(std::uintptr_t base)
        {
            const std::uintptr_t controller = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
            NLOG("[inv] controller=%p", (void*)controller);
            if (!controller) return;

            const std::uintptr_t inv = Mem::Read<std::uintptr_t>(controller + kInvServices);
            NLOG("[inv] inventoryServices=%p", (void*)inv);
            if (!inv) return;

            const int            count = Mem::Read<int>(inv + kLoadoutVec + 0x0);
            const std::uintptr_t data  = Mem::Read<std::uintptr_t>(inv + kLoadoutVec + 0x8);
            NLOG("[inv] loadout count=%d data=%p", count, (void*)data);
            if (!data || count <= 0 || count > 128) return;

            for (int i = 0; i < count && i < 64; ++i)
            {
                const std::uintptr_t pA = Mem::Read<std::uintptr_t>(data + kSlotStrideA * i + kSlotItemPtr);
                const std::uintptr_t pB = Mem::Read<std::uintptr_t>(data + kSlotStrideB * i + kSlotItemPtr);
                const std::uint16_t  dA = pA ? Mem::Read<std::uint16_t>(pA + kDefIndex) : 0;
                const std::uint16_t  dB = pB ? Mem::Read<std::uint16_t>(pB + kDefIndex) : 0;
                NLOG("[inv] slot%d  A:item=%p def=%u | B:item=%p def=%u", i, (void*)pA, dA, (void*)pB, dB);
            }
        }

        void GiveKnife(std::uintptr_t base, std::ptrdiff_t stride)
        {
            const std::uintptr_t controller = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
            if (!controller) return;
            const std::uintptr_t inv = Mem::Read<std::uintptr_t>(controller + kInvServices);
            if (!inv) return;
            const int            count = Mem::Read<int>(inv + kLoadoutVec + 0x0);
            const std::uintptr_t data  = Mem::Read<std::uintptr_t>(inv + kLoadoutVec + 0x8);
            if (!data || count <= 0 || count > 128) return;

            for (int i = 0; i < count && i < 64; ++i)
            {
                const std::uintptr_t item = Mem::Read<std::uintptr_t>(data + stride * i + kSlotItemPtr);
                if (!item) continue;
                const std::uint16_t def = Mem::Read<std::uint16_t>(item + kDefIndex);
                if (!IsKnife(def)) continue;

                Mem::Write<std::uint16_t>(item + kDefIndex, 515);
                Mem::Write<int>(item + kQuality, 3);
                Mem::Write<std::uint32_t>(item + kItemIDHigh, 0xFFFFFFFF);
                Mem::Write<std::uint32_t>(item + kItemIDLow, 0xFFFFFFFF);
                Mem::Write<bool>(item + kInit, true);
                NLOG("[inv] knife slot%d def %u -> 515 applied", i, def);
            }
        }

        void Worker()
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            NLOG("[inv] worker start (after 5s)");

            for (int t = 0; t < 10 && g_running.load(); ++t)
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base) DumpLoadout(base);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            while (g_running.load())
            {
                const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
                if (base) GiveKnife(base, kSlotStrideA);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }

    void Start() { if (g_running.exchange(true)) return; g_worker = std::thread(Worker); }
    void Stop()  { if (!g_running.exchange(false)) return; if (g_worker.joinable()) g_worker.join(); }
}
