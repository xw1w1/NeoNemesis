#include "EnemyChams.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include <cstdint>

namespace Nemesis::EnemyChams
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::uintptr_t EntFromHandle(std::uintptr_t base, std::uint32_t h)
        {
            if (h == 0xFFFFFFFF)
                return 0;
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list)
                return 0;
            const std::uint32_t i = h & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
        }

        std::uintptr_t EntByIndex(std::uintptr_t base, std::uint32_t i)
        {
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list)
                return 0;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk)
                return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
        }
    }

    void Render()
    {
        if (!Enabled)
            return;

        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base)
            return;

        const std::uintptr_t localPawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
        if (!localPawn)
            return;

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);

        for (std::uint32_t i = 1; i <= 64; ++i)
        {
            const std::uintptr_t controller = EntByIndex(base, i);
            if (!controller)
                continue;

            const std::uint32_t ph = Mem::Read<std::uint32_t>(controller + Schema::m_hPlayerPawn);
            const std::uintptr_t pawn = EntFromHandle(base, ph);
            if (!pawn || pawn == localPawn)
                continue;

            const int hp = Mem::Read<int>(pawn + Schema::m_iHealth, 0);
            if (hp <= 0 || hp > 100)
                continue;

            const std::uint8_t team = Mem::Read<std::uint8_t>(pawn + Schema::m_iTeamNum);
            if (team < 2 || team == localTeam)
                continue;

            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!scene)
                continue;

            Mem::Write<std::uint32_t>(scene + Schema::m_clrRender, TintRGBA);
        }
    }
}
