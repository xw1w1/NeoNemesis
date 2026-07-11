#include "LegitBot.hpp"
#include "LegitBotConfig.hpp"
#include "../../../Miscellaneous Utilities/Visibility Check System/Visibility.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include "imgui.h"

#include <cstdint>
#include <cmath>
#include <chrono>
#include <Windows.h>

#include "../Miscellaneous Functions/AimConfig.h"

namespace Nemesis::LegitBot
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr std::ptrdiff_t kBoneArray   = 0x1D0;
        constexpr std::ptrdiff_t kBoneStride  = 0x20;
        constexpr int            kHeadBone    = 6;
        constexpr float          kRecoilEps   = 0.05f;

        struct Vec3 { float x, y, z; };

        Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
        {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
        }

        bool PartEnabled(int id)
        {
            switch (id)
            {
                case 0:  return Config::aimHead;
                case 1:  return Config::aimChest;
                case 2:  return Config::aimStomach;
                case 3:  return Config::aimHands;
                default: return Config::aimLegs;
            }
        }

        Vec3 PartPoint(int id, const Vec3& head, const Vec3& feet)
        {
            switch (id)
            {
                case 0:  return head;
                case 1:  return Lerp(feet, head, 0.83f);
                case 2:  return Lerp(feet, head, 0.60f);
                case 3:  return Lerp(feet, head, 0.72f);
                default: return Lerp(feet, head, 0.25f);
            }
        }

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

        bool IsKnife(std::uint16_t d)
        {
            return d == 42 || d == 59 || (d >= 500 && d <= 525);
        }

        bool W2S(const float* m, const Vec3& p, float w, float h, float& sx, float& sy)
        {
            const float cw = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
            if (cw < 0.01f)
                return false;
            const float cx = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
            const float cy = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
            sx = (w * 0.5f) * (1.0f + cx / cw);
            sy = (h * 0.5f) * (1.0f - cy / cw);
            return true;
        }

        bool HeadBone(std::uintptr_t pawn, Vec3& out)
        {
            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!scene)
                return false;
            const std::uintptr_t bones = Mem::Read<std::uintptr_t>(scene + kBoneArray);
            if (bones < 0x10000)
                return false;
            const std::uintptr_t b = bones + kHeadBone * kBoneStride;
            out.x = Mem::Read<float>(b + 0);
            out.y = Mem::Read<float>(b + 4);
            out.z = Mem::Read<float>(b + 8);
            return out.x != 0.0f || out.y != 0.0f || out.z != 0.0f;
        }
    }

    void Render()
    {
        const float kFovRadius = g_AimConfig.aimbotFov;
        const std::uintptr_t base = Mem::ModuleBase(Modules::kClient);
        if (!base)
            return;

        const std::uintptr_t localPawn = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerPawn);
        if (!localPawn)
            return;
        if (Mem::Read<int>(localPawn + Schema::m_iHealth, 0) <= 0)
            return;

        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 center(ds.x * 0.5f, ds.y * 0.5f);
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        std::uintptr_t activeWeapon = 0;
        bool holdingGun = false;
        const std::uintptr_t ws = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pWeaponServices);
        if (ws)
        {
            const std::uintptr_t active = EntFromHandle(base, Mem::Read<std::uint32_t>(ws + Schema::m_hActiveWeapon));
            if (active)
            {
                const std::uint16_t def = Mem::Read<std::uint16_t>(
                    active + Schema::m_AttributeManager + Schema::m_Item + Schema::m_iItemDefinitionIndex);
                holdingGun = (def != 0 && !IsKnife(def));
                activeWeapon = active;
            }
        }

        if (holdingGun)
            dl->AddCircle(center, kFovRadius, IM_COL32(255, 255, 255, 200), 96, 1.4f);

        if (!holdingGun)
            return;

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);

        const std::uintptr_t localController = Mem::Read<std::uintptr_t>(base + Client::dwLocalPlayerController);
        int localSlot = -1;
        for (std::uint32_t i = 1; i <= 64; ++i)
            if (EntByIndex(base, i) == localController) { localSlot = static_cast<int>(i) - 1; break; }

        float vm[16];
        {
            const std::uintptr_t mp = base + Client::dwViewMatrix;
            for (int k = 0; k < 16; ++k)
                vm[k] = Mem::Read<float>(mp + k * 4);
        }

        static int s_lockPawn = -1;
        static int s_lockPart = -1;

        int    chosenPawn = -1, chosenPart = -1;
        float  chosenDist = kFovRadius;
        Vec3   chosenPoint{};
        ImVec2 chosenScreen(0.0f, 0.0f);

        bool   lockValid = false;
        Vec3   lockPoint{};
        ImVec2 lockScreen(0.0f, 0.0f);

        for (std::uint32_t i = 1; i <= 64 && localSlot >= 0; ++i)
        {
            const std::uintptr_t controller = EntByIndex(base, i);
            if (!controller)
                continue;
            const std::uint32_t pawnHandle = Mem::Read<std::uint32_t>(controller + Schema::m_hPlayerPawn);
            const std::uintptr_t pawn = EntFromHandle(base, pawnHandle);
            if (!pawn || pawn == localPawn)
                continue;
            const int hp = Mem::Read<int>(pawn + Schema::m_iHealth, 0);
            if (hp <= 0 || hp > 100)
                continue;
            const std::uint8_t team = Mem::Read<std::uint8_t>(pawn + Schema::m_iTeamNum);
            if (team < 2 || team == localTeam)
                continue;

            const std::uint64_t mask = Mem::Read<std::uint64_t>(
                pawn + Schema::m_entitySpottedState + Schema::m_bSpottedByMask, 0);
            if (((mask >> localSlot) & 1ull) == 0)
                continue;

            const std::uintptr_t scene = Mem::Read<std::uintptr_t>(pawn + Schema::m_pGameSceneNode);
            if (!scene)
                continue;
            Vec3 head;
            if (!HeadBone(pawn, head))
                continue;
            Vec3 feet;
            feet.x = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
            feet.y = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
            feet.z = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8);

            const int pawnIdx = static_cast<int>(pawnHandle & EntityList::kIndexMask);

            int    bestPart = -1;
            float  bestPd = kFovRadius;
            Vec3   bestPp{};
            ImVec2 bestPs(0.0f, 0.0f);
            for (int id = 0; id < 5; ++id)
            {
                if (!PartEnabled(id))
                    continue;
                const Vec3 p = PartPoint(id, head, feet);
                float sx, sy;
                if (!W2S(vm, p, ds.x, ds.y, sx, sy))
                    continue;
                const float dx = sx - center.x, dy = sy - center.y;
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d < bestPd) { bestPd = d; bestPart = id; bestPp = p; bestPs = ImVec2(sx, sy); }
            }
            if (bestPart < 0)
                continue;

            if (pawnIdx == s_lockPawn && s_lockPart >= 0 && PartEnabled(s_lockPart))
            {
                const Vec3 lp = PartPoint(s_lockPart, head, feet);
                float lsx, lsy;
                if (W2S(vm, lp, ds.x, ds.y, lsx, lsy))
                {
                    const float dx = lsx - center.x, dy = lsy - center.y;
                    if (std::sqrt(dx * dx + dy * dy) <= kFovRadius)
                    {
                        lockValid = true;
                        lockPoint = lp;
                        lockScreen = ImVec2(lsx, lsy);
                    }
                }
            }

            if (bestPd < chosenDist)
            {
                chosenDist = bestPd;
                chosenPawn = pawnIdx;
                chosenPart = bestPart;
                chosenPoint = bestPp;
                chosenScreen = bestPs;
            }
        }

        bool   haveBest = false;
        bool   seen = false;
        Vec3   bestPoint{};
        ImVec2 bestScreen(0.0f, 0.0f);
        int    bestPawnIndex = -1;

        if (lockValid)
        {
            haveBest = true; seen = true;
            bestPoint = lockPoint; bestScreen = lockScreen; bestPawnIndex = s_lockPawn;
        }
        else if (chosenPawn >= 0)
        {
            s_lockPawn = chosenPawn; s_lockPart = chosenPart;
            haveBest = true; seen = true;
            bestPoint = chosenPoint; bestScreen = chosenScreen; bestPawnIndex = chosenPawn;
        }
        else
        {
            s_lockPawn = -1; s_lockPart = -1;
        }

        Vec3 aimPoint = bestPoint;

        if (haveBest)
        {
            const ImU32 col = seen ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
            dl->AddCircleFilled(bestScreen, 3.5f, col);
            dl->AddCircle(bestScreen, 5.0f, IM_COL32(255, 255, 0, 255), 16, 1.5f);
        }

        if (seen)
        {
            const std::uintptr_t lscene = Mem::Read<std::uintptr_t>(localPawn + Schema::m_pGameSceneNode);
            if (lscene)
            {
                const float ex = Mem::Read<float>(lscene + Schema::m_vecAbsOrigin + 0);
                const float ey = Mem::Read<float>(lscene + Schema::m_vecAbsOrigin + 4);
                const float ez = Mem::Read<float>(lscene + Schema::m_vecAbsOrigin + 8)
                               + Mem::Read<float>(localPawn + Schema::m_vecViewOffset + 8);

                const float ddx = aimPoint.x - ex;
                const float ddy = aimPoint.y - ey;
                const float ddz = aimPoint.z - ez;
                const float flat = std::sqrt(ddx * ddx + ddy * ddy);

                constexpr float kRad = 57.2957795131f;
                const float tgtYaw   = std::atan2(ddy, ddx) * kRad;
                const float tgtPitch = -std::atan2(ddz, flat) * kRad;

                const std::uintptr_t va = base + Client::dwViewAngles;
                const float curPitch = Mem::Read<float>(va + 0);
                const float curYaw   = Mem::Read<float>(va + 4);

                float dYaw = tgtYaw - curYaw;
                while (dYaw > 180.0f)  dYaw -= 360.0f;
                while (dYaw < -180.0f) dYaw += 360.0f;
                float dPitch = tgtPitch - curPitch;

                static std::chrono::steady_clock::time_point s_lastAim = std::chrono::steady_clock::now();
                const auto tAim = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(tAim - s_lastAim).count();
                s_lastAim = tAim;
                if (dt > 0.1f) dt = 0.1f;

                const float ease = 1.0f - std::exp(-Config::aimSmooth * dt);
                float sYaw   = dYaw * ease;
                float sPitch = dPitch * ease;

                const float maxStep = Config::aimSpeed * dt;
                const float mag = std::sqrt(sYaw * sYaw + sPitch * sPitch);
                if (mag > maxStep && mag > 0.0001f)
                {
                    const float s = maxStep / mag;
                    sYaw *= s;
                    sPitch *= s;
                }

                float nYaw   = curYaw + sYaw;
                float nPitch = curPitch + sPitch;
                if (nPitch > 89.0f)  nPitch = 89.0f;
                if (nPitch < -89.0f) nPitch = -89.0f;

                Mem::Write<float>(va + 0, nPitch);
                Mem::Write<float>(va + 4, nYaw);
            }
        }

        const int crosshairId = Mem::Read<int>(localPawn + Schema::m_iIDEntIndex, -1);
        const bool onTarget = seen && haveBest && crosshairId == bestPawnIndex;

        float recoil = 0.0f;
        if (activeWeapon)
            recoil = Mem::Read<float>(activeWeapon + Weapon::m_flRecoilIndex, 0.0f);
        const int shots = Mem::Read<int>(localPawn + PawnCombat::m_iShotsFired, 0);

        static bool s_hadTarget = false;
        static std::chrono::steady_clock::time_point s_since;
        static int s_fireState = 0;
        static int s_startShots = 0;
        const auto now = std::chrono::steady_clock::now();

        if (onTarget)
        {
            if (!s_hadTarget) { s_hadTarget = true; s_since = now; }
        }
        else
        {
            s_hadTarget = false;
        }

        const double heldMs = s_hadTarget
            ? std::chrono::duration<double, std::milli>(now - s_since).count() : 0.0;
        const bool reactionOk = s_hadTarget && heldMs >= Config::reactionMs;

        if (s_fireState == 0)
        {
            if (reactionOk && onTarget && recoil <= kRecoilEps)
            {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                s_startShots = shots;
                s_fireState = 1;
            }
        }
        else if (s_fireState == 1)
        {
            if (!onTarget || (shots - s_startShots) >= Config::burstShots)
            {
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                s_fireState = 2;
            }
        }
        else
        {
            if (recoil <= kRecoilEps)
                s_fireState = 0;
        }
    }
}
