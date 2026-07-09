#include "RageBot.hpp"
#include "SilentAim.hpp"
#include "../../../Server Packet Anomaly Guard/Search for Anomalies in Packages/PacketGuard.hpp"
#include "../../../Miscellaneous Utilities/Visibility Check System/Visibility.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include "imgui.h"

#include <cstdint>
#include <cmath>

namespace Nemesis::RageBot
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr float          kRageFov    = 45.0f;  // радиус красного FOV (px)
        constexpr std::ptrdiff_t kBoneArray  = 0x1D0;
        constexpr std::ptrdiff_t kBoneStride = 0x20;
        constexpr int            kHeadBone   = 6;

        struct Vec3 { float x, y, z; };

        std::uintptr_t EntFromHandle(std::uintptr_t base, std::uint32_t h)
        {
            if (h == 0xFFFFFFFF) return 0;
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list) return 0;
            const std::uint32_t i = h & EntityList::kIndexMask;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk) return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
        }

        std::uintptr_t EntByIndex(std::uintptr_t base, std::uint32_t i)
        {
            const std::uintptr_t list = Mem::Read<std::uintptr_t>(base + Client::dwEntityList);
            if (!list) return 0;
            const std::uintptr_t chunk = Mem::Read<std::uintptr_t>(
                list + EntityList::kChunkStep * (i >> EntityList::kChunkShift) + EntityList::kChunkBase);
            if (!chunk) return 0;
            return Mem::Read<std::uintptr_t>(chunk + EntityList::kEntryStride * (i & EntityList::kSlotMask));
        }

        bool IsKnife(std::uint16_t d)
        {
            return d == 42 || d == 59 || (d >= 500 && d <= 525);
        }

        bool W2S(const float* m, const Vec3& p, float w, float h, float& sx, float& sy)
        {
            const float cw = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
            if (cw < 0.01f) return false;
            const float cx = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
            const float cy = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
            sx = (w * 0.5f) * (1.0f + cx / cw);
            sy = (h * 0.5f) * (1.0f - cy / cw);
            return true;
        }

        bool HeadBone(std::uintptr_t pawn, Vec3& out)
        {
            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!scene) return false;
            const std::uintptr_t bones = Mem::Read<std::uintptr_t>(scene + kBoneArray);
            if (bones < 0x10000) return false;
            const std::uintptr_t b = bones + kHeadBone * kBoneStride;
            out.x = Mem::Read<float>(b + 0);
            out.y = Mem::Read<float>(b + 4);
            out.z = Mem::Read<float>(b + 8);
            return out.x != 0.0f || out.y != 0.0f || out.z != 0.0f;
        }
    }

    void Render()
    {
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base) return;

        const std::uintptr_t localPawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
        if (!localPawn) return;
        if (Mem::Read<int>(localPawn + Schema::m_iHealth, 0) <= 0) return;

        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 center(ds.x * 0.5f, ds.y * 0.5f);
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // держим оружие (не нож)?
        const std::uintptr_t ws = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pWeaponServices);
        bool holdingGun = false;
        if (ws)
        {
            const std::uintptr_t active = EntFromHandle(base, Mem::Read<std::uint32_t>(ws + Schema::m_hActiveWeapon));
            if (active)
            {
                const std::uint16_t def = Mem::Read<std::uint16_t>(
                    active + Schema::m_AttributeManager + Schema::m_Item + Schema::m_iItemDefinitionIndex);
                holdingGun = (def != 0 && !IsKnife(def));
            }
        }
        if (!holdingGun) return;

        // КРАСНЫЙ FOV
        dl->AddCircle(center, kRageFov, IM_COL32(155, 40, 40, 90), 96, 3.0f);

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);
        float vm[16];
        for (int k = 0; k < 16; ++k)
            vm[k] = Mem::Read<float>(base + Client::dwViewMatrix + k * 4);

        float  bestDist = kRageFov;
        ImVec2 bestScreen(0, 0);
        int    bestPawnIndex = -1;
        Vec3   bestHead{};
        Vec3   bestAbs{};
        bool   haveBest = false;

        for (std::uint32_t i = 1; i <= 64; ++i)
        {
            const std::uintptr_t controller = EntByIndex(base, i);
            if (!controller) continue;
            const std::uint32_t pawnHandle = Mem::Read<std::uint32_t>(controller + Schema::m_hPlayerPawn);
            const std::uintptr_t pawn = EntFromHandle(base, pawnHandle);
            if (!pawn || pawn == localPawn) continue;
            const int hp = Mem::Read<int>(pawn + Schema::m_iHealth, 0);
            if (hp <= 0 || hp > 100) continue;
            const std::uint8_t team = Mem::Read<std::uint8_t>(pawn + Schema::m_iTeamNum);
            if (team < 2 || team == localTeam) continue;

            Vec3 head;
            if (!HeadBone(pawn, head)) continue;

            Vec3 origin{};
            const std::uintptr_t sc = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (sc)
            {
                origin.x = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 0);
                origin.y = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 4);
                origin.z = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 8);
            }

            float sx, sy;
            if (!W2S(vm, head, ds.x, ds.y, sx, sy)) continue;
            const float dx = sx - center.x, dy = sy - center.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < bestDist)
            {
                bestDist = d;
                bestScreen = ImVec2(sx, sy);
                bestPawnIndex = static_cast<int>(pawnHandle & EntityList::kIndexMask);
                bestHead = head;
                bestAbs = origin;
                haveBest = true;
            }
        }

        // видимость трейсом (LOS)
        const bool visible = haveBest &&
            Nemesis::Visibility::VisibleToLocal(base, localPawn, bestPawnIndex);

        if (haveBest)
        {
            const ImU32 col = visible ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
            dl->AddCircleFilled(bestScreen, 3.5f, col);
        }

                if (visible)
        {
            Nemesis::SilentAim::SetTarget(bestHead.x, bestHead.y, bestHead.z);
            Nemesis::PacketGuard::SetTarget(bestHead.x, bestHead.y, bestHead.z, bestAbs.x, bestAbs.y, bestAbs.z);
        }
        else
        {
            Nemesis::SilentAim::ClearTarget();
            Nemesis::PacketGuard::ClearTarget();
        }
    }
}