#include "Esp.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"

#include "imgui.h"
#include "../../../RenderDrx11/RenderHook.hpp"

#include <cstdint>
#include <cmath>
#include <cfloat>

namespace Nemesis::Esp
{
    using namespace Nemesis::Addresses;

    namespace
    {
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

        bool W2S(std::uintptr_t mat, const Vec3& p, float w, float h, float& sx, float& sy)
        {
            float m[16];
            for (int i = 0; i < 16; ++i)
                m[i] = Mem::Read<float>(mat + i * 4);

            const float cw = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
            if (cw < 0.01f)
                return false;

            const float cx = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
            const float cy = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
            sx = (w * 0.5f) * (1.0f + cx / cw);
            sy = (h * 0.5f) * (1.0f - cy / cw);
            return true;
        }

        ImU32 Blend(float tt)
        {
            const float r = 246.0f + (239.0f - 246.0f) * tt;
            const float g = 183.0f + (113.0f - 183.0f) * tt;
            const float b = 167.0f + (137.0f - 167.0f) * tt;
            return IM_COL32(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), 255);
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

        const int localHp = Mem::Read<int>(localPawn + Schema::m_iHealth, 0);
        if (localHp <= 0)
            return;

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);
        const std::uintptr_t mat = base + Client::dwViewMatrix;
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        const float gt = static_cast<float>(ImGui::GetTime()) * 2.2f;
        const ImU32 col = Blend(0.5f + 0.5f * std::sin(gt));

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

            Vec3 origin;
            origin.x = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 0);
            origin.y = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 4);
            origin.z = Mem::Read<float>(scene + Schema::m_vecAbsOrigin + 8);
            const Vec3 head = { origin.x, origin.y, origin.z + 72.0f };

            float fx, fy, hx, hy;
            if (!W2S(mat, origin, ds.x, ds.y, fx, fy))
                continue;
            if (!W2S(mat, head, ds.x, ds.y, hx, hy))
                continue;

            const float boxH = fy - hy;
            if (boxH < 1.0f)
                continue;

            const float boxW = boxH * 0.34f;
            const float x   = hx - boxW * 0.5f - 2.5f;
            const float x2  = hx + boxW * 0.5f + 2.5f;
            const float top = hy - 3.0f;
            const float R   = 3.0f;
            const float th  = 2.3f;

            dl->AddRect(ImVec2(x - 1.0f, top - 1.0f), ImVec2(x2 + 1.0f, fy + 1.0f), IM_COL32(0, 0, 0, 170), R, 0, 1.5f);
            dl->AddRect(ImVec2(x, top), ImVec2(x2, fy), col, R, 0, th);

            char name[32];
            for (int j = 0; j < 31; ++j)
                name[j] = Mem::Read<char>(controller + 0x6F4 + j);
            name[31] = 0;
            if (name[0])
            {
                ImFont* font = Nemesis::RenderHook::GetEspFont();
                const float fs = 15.0f;
                const ImVec2 ts = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.0f, name) : ImGui::CalcTextSize(name);
                const float nx = (x + x2) * 0.5f - ts.x * 0.5f;
                const float ny = top - ts.y - 4.0f;
                if (font)
                {
                    dl->AddText(font, fs, ImVec2(nx + 1.0f, ny + 1.0f), IM_COL32(0, 0, 0, 200), name);
                    dl->AddText(font, fs, ImVec2(nx, ny), col, name);
                }
                else
                {
                    dl->AddText(ImVec2(nx + 1.0f, ny + 1.0f), IM_COL32(0, 0, 0, 200), name);
                    dl->AddText(ImVec2(nx, ny), col, name);
                }
            }
        }
    }
}
