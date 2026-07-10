#include "RageBot.hpp"
#include "SilentAim.hpp"
#include "../../../Server Packet Anomaly Guard/Search for Anomalies in Packages/PacketGuard.hpp"
#include "../../../Miscellaneous Utilities/Visibility Check System/Visibility.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include "imgui.h"

#include <cstdint>
#include <cmath>
#include <Windows.h>

#include "../Miscellaneous Functions/AimConfig.h"

namespace Nemesis::RageBot
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr std::ptrdiff_t kBoneArray  = Bones::kBoneArray;
        constexpr std::ptrdiff_t kBoneStride = Bones::kBoneStride;
        constexpr int            kHeadBone   = Bones::kHeadBone;

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
        const float kRageFov = g_AimConfig.aimbotFov;
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

        // Видимость через m_bSpottedByMask (трейс мёртв): бит локального слота = личная видимость.
        const std::uintptr_t localController = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
        int localSlot = -1;
        for (std::uint32_t i = 1; i <= 64; ++i)
            if (EntByIndex(base, i) == localController) { localSlot = static_cast<int>(i) - 1; break; }

        float vm[16];
        for (int k = 0; k < 16; ++k)
            vm[k] = Mem::Read<float>(base + Client::dwViewMatrix + k * 4);

        // Метки считаются для ВСЕХ врагов каждый кадр (мгновенно); цель — если пиксель головы в FOV.
        float  bestDist = kRageFov;
        ImVec2 bestScreen(0, 0);
        int    bestPawnIndex = -1;
        Vec3   bestHead{};
        Vec3   bestAbs{};
        bool   haveBest = false;
        int    dbgEnemies = 0, dbgSpotted = 0, dbgFov = 0;
        float  dbgMinD = 1e9f;

        for (std::uint32_t i = 1; i <= 64 && localSlot >= 0; ++i)
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
            ++dbgEnemies;

            const std::uint64_t mask = Mem::Read<std::uint64_t>(
                pawn + Schema::m_entitySpottedState + Schema::m_bSpottedByMask, 0);
            if (((mask >> localSlot) & 1ull) == 0) continue;
            ++dbgSpotted;

            const std::uintptr_t sc = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!sc) continue;
            Vec3 origin{};
            origin.x = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 0);
            origin.y = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 4);
            origin.z = Mem::Read<float>(sc + Schema::m_vecAbsOrigin + 8);

            // Голова = origin(ноги) + высота глаз (viewOffset.z, фолбэк) — надёжно, как ESP, без bone-массива.
            float eyeZ = Mem::Read<float>(pawn + Schema::m_vecViewOffset + 8, 0.0f);
            if (!(eyeZ > 30.0f && eyeZ < 90.0f)) eyeZ = 64.0f;
            const Vec3 hc{ origin.x, origin.y, origin.z + eyeZ };

            // Мультиточка по хитбоксу головы: облако точек, берём пиксель ближайший к прицелу
            // (обход края FOV/частичного перекрытия). Рендерим потом только выбранный.
            const float R = AimFn::kHeadRadius;
            const Vec3 cloud[7] = {
                hc,
                { hc.x + R, hc.y, hc.z }, { hc.x - R, hc.y, hc.z },
                { hc.x, hc.y + R, hc.z }, { hc.x, hc.y - R, hc.z },
                { hc.x, hc.y, hc.z + R }, { hc.x, hc.y, hc.z - R },
            };
            float  ed = 1e9f; Vec3 ep{}; ImVec2 es(0, 0); bool eHave = false;
            for (const Vec3& p : cloud)
            {
                float px, py;
                if (!W2S(vm, p, ds.x, ds.y, px, py)) continue;
                const float ex = px - center.x, ey = py - center.y;
                const float dd = std::sqrt(ex * ex + ey * ey);
                if (dd < ed) { ed = dd; ep = p; es = ImVec2(px, py); eHave = true; }
            }
            if (!eHave) continue;
            if (ed < dbgMinD) dbgMinD = ed;
            if (ed < kRageFov) ++dbgFov;
            if (ed < bestDist)
            {
                bestDist = ed;
                bestScreen = es;
                bestPawnIndex = static_cast<int>(pawnHandle & EntityList::kIndexMask);
                bestHead = ep;
                bestAbs = origin;
                haveBest = true;
            }
        }

        {
            static DWORD s_rdbg = 0;
            if (Nemesis::SilentAim::Enabled() && GetTickCount() - s_rdbg > 400)
            {
                s_rdbg = GetTickCount();
                NLOG("[rage] localSlot=%d enemies=%d spotted=%d inFov(%.0f)=%d minD=%.0f haveBest=%d",
                     localSlot, dbgEnemies, dbgSpotted, kRageFov, dbgFov, dbgMinD, haveBest ? 1 : 0);
            }
        }

        // Видимость уже отфильтрована в цикле по m_bSpottedByMask → haveBest == видимая цель.
        const bool visible = haveBest;

        if (haveBest)
            dl->AddCircleFilled(bestScreen, 3.5f, IM_COL32(0, 255, 0, 255));

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