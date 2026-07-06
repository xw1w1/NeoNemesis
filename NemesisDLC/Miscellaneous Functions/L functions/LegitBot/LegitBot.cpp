#include "LegitBot.hpp"
#include "LegitBotConfig.hpp"
#include "../../../Miscellaneous Utilities/Visibility Check System/Visibility.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include "imgui.h"

#include <DirectXMath.h>

#include <cstdint>
#include <cmath>
#include <vector>
#include <Windows.h>

namespace Nemesis::LegitBot
{
    using namespace Nemesis::Addresses;

    namespace
    {
        constexpr float          kFovRadius   = Config::fovRadius;
        constexpr std::ptrdiff_t kBoneArray   = 0x1D0; // sceneNode -> bone transform array (m_modelState + 0x80)
        constexpr std::ptrdiff_t kBoneStride  = 0x20;  // size of one bone transform
        constexpr int            kHeadBone    = 6;     // standard player head bone

        struct Vec3 { float x, y, z; };

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

        if (holdingGun)
        {
            dl->AddCircle(center, kFovRadius, IM_COL32(255, 255, 255, 200), 96, 1.4f);
        }

        if (!holdingGun)
            return;

        static std::vector<Vec3> g_head;
        if (g_head.empty())
        {
            if (Config::aimHead)
                for (float x = -3.5f; x <= 3.5f; x += 0.6f)
                    for (float y = -3.5f; y <= 3.5f; y += 0.6f)
                        for (float z = -3.5f; z <= 3.5f; z += 0.6f)
                            if (x * x + y * y + z * z <= 12.25f)
                                g_head.push_back({ x, y, z });
            if (Config::aimBody)
                for (float x = -7.0f; x <= 7.0f; x += 1.5f)
                    for (float y = -7.0f; y <= 7.0f; y += 1.5f)
                        for (float z = -44.0f; z <= -8.0f; z += 2.5f)
                            g_head.push_back({ x, y, z });
        }

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);
        float vm[16];
        {
            const std::uintptr_t mp = base + Client::dwViewMatrix;
            for (int k = 0; k < 16; ++k)
                vm[k] = Mem::Read<float>(mp + k * 4);
        }

        float   bestDist = kFovRadius;
        ImVec2  bestScreen(0.0f, 0.0f);
        Vec3    bestPoint{};
        int     bestPawnIndex = -1;
        bool    haveBest = false;

        for (std::uint32_t i = 1; i <= 64; ++i)
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

            Vec3 head;
            if (!HeadBone(pawn, head))
                continue;

            for (const Vec3& o : g_head)
            {
                const Vec3 p = { head.x + o.x, head.y + o.y, head.z + o.z };
                float sx, sy;
                if (!W2S(vm, p, ds.x, ds.y, sx, sy))
                    continue;

                const float dx = sx - center.x;
                const float dy = sy - center.y;
                const float d = std::sqrt(dx * dx + dy * dy);
                if (d < bestDist)
                {
                    bestDist = d;
                    bestScreen = ImVec2(sx, sy);
                    bestPoint = p;
                    bestPawnIndex = static_cast<int>(pawnHandle & EntityList::kIndexMask);
                    haveBest = true;
                }
            }
        }

        bool seen = false;
        if (haveBest)
        {
            const std::uintptr_t bestPawn = EntByIndex(base, static_cast<std::uint32_t>(bestPawnIndex));
            if (bestPawn)
                seen = Mem::Read<bool>(bestPawn + Schema::m_entitySpottedState + Schema::m_bSpotted);
        }

        if (haveBest)
        {
            const ImU32 col = seen ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
            dl->AddCircleFilled(bestScreen, 3.5f, col);
            dl->AddCircle(bestScreen, 5.0f, IM_COL32(255, 255, 0, 255), 16, 1.5f);
        }

        static bool s_firing = false;

        // FOV / автоприцел: тянем мышь только когда врага реально видно
        if (seen)
        {
            const int mx = static_cast<int>((bestScreen.x - center.x) * Config::aimSpeed);
            const int my = static_cast<int>((bestScreen.y - center.y) * Config::aimSpeed);
            if (mx != 0 || my != 0)
                mouse_event(MOUSEEVENTF_MOVE, mx, my, 0, 0);
        }

        // Стрельба: только когда прицел реально наведён на врага (crosshair-ID)
        const int crosshairId = Mem::Read<int>(localPawn + Schema::m_iIDEntIndex, -1);
        const bool onTarget = haveBest && crosshairId == bestPawnIndex;
        if (onTarget && !s_firing)
        {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            s_firing = true;
        }
        else if (!onTarget && s_firing)
        {
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            s_firing = false;
        }
      
    }
}
