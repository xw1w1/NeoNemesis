#include "WallHackV2.hpp"
#include "WallHackV2Config.hpp"
#include "../../UnusualNewVisions/CameraPositionChange/Memory.hpp"
#include "../../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../../RenderDrx11/RenderHook.hpp"
#include "../../../System API Rendering/SystemShaderBloom/SystemShaderBloom.hpp"

#include "imgui.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <memory>
#include <cstdint>
#include <cmath>

namespace Nemesis::WallHackV2
{
    using namespace Nemesis::Addresses;
    namespace Bloom = Nemesis::SystemShaderBloom;

    namespace
    {
        struct Vec3 { float x, y, z; };

        std::unique_ptr<Bloom::BloomEffect> g_bloom;
        ID3D11Texture2D*          g_tex = nullptr;
        ID3D11RenderTargetView*   g_rtv = nullptr;
        ID3D11ShaderResourceView* g_srv = nullptr;
        int g_w = 0;
        int g_h = 0;

        void ReleaseResources()
        {
            if (g_srv) { g_srv->Release(); g_srv = nullptr; }
            if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
            if (g_tex) { g_tex->Release(); g_tex = nullptr; }
            g_bloom.reset();
            g_w = g_h = 0;
        }

        bool g_bloomFailed = false;

        bool EnsureResources(ID3D11Device* device, ID3D11DeviceContext* context, int w, int h)
        {
            if (w <= 0 || h <= 0)
                return false;
            if (g_srv && g_bloom && g_w == w && g_h == h)
                return true;
            if (g_bloomFailed)
                return false;

            ReleaseResources();

            D3D11_TEXTURE2D_DESC td{};
            td.Width = static_cast<UINT>(w);
            td.Height = static_cast<UINT>(h);
            td.MipLevels = 1;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

            if (FAILED(device->CreateTexture2D(&td, nullptr, &g_tex)))
                return false;
            if (FAILED(device->CreateRenderTargetView(g_tex, nullptr, &g_rtv)))
            {
                ReleaseResources();
                return false;
            }
            if (FAILED(device->CreateShaderResourceView(g_tex, nullptr, &g_srv)))
            {
                ReleaseResources();
                return false;
            }

            g_bloom = Bloom::CreateBloomEffect(device, context,
                                               static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            if (!g_bloom || !g_bloom->IsInitialized())
            {
                ReleaseResources();
                return false;
            }

            g_w = w;
            g_h = h;
            return true;
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

        void BuildSilhouettes(ImDrawList* dl, std::uintptr_t base, std::uintptr_t localPawn,
                              std::uint8_t localTeam, std::uintptr_t mat, const ImVec2& ds)
        {
            const ImU32 col = IM_COL32(Config::kColorR, Config::kColorG, Config::kColorB, Config::kColorA);

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

                const float bodyH = fy - hy;
                if (bodyH < 2.0f)
                    continue;

                const float w      = bodyH * Config::kBodyWidthScale;
                const float headR  = w * Config::kHeadRadiusScale;
                const float cx     = (fx + hx) * 0.5f;
                const float headCy = hy + headR;

                const ImVec2 bodyTop(cx - w * 0.5f, headCy + headR * 0.55f);
                const ImVec2 bodyBot(cx + w * 0.5f, fy);
                const float  rounding = w * 0.45f;

                dl->AddRectFilled(bodyTop, bodyBot, col, rounding);
                dl->AddCircleFilled(ImVec2(cx, headCy), headR, col);
            }
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

        const std::uint8_t localTeam = Mem::Read<std::uint8_t>(localPawn + Schema::m_iTeamNum);
        const std::uintptr_t mat = base + Client::dwViewMatrix;
        const ImVec2 ds = ImGui::GetIO().DisplaySize;

        ID3D11Device*        device  = Nemesis::RenderHook::GetDevice();
        ID3D11DeviceContext* context = Nemesis::RenderHook::GetContext();
        const int w = static_cast<int>(ds.x);
        const int h = static_cast<int>(ds.y);

        if (!device || !context || !EnsureResources(device, context, w, h))
        {
            BuildSilhouettes(ImGui::GetForegroundDrawList(), base, localPawn, localTeam, mat, ds);
            return;
        }

        ImDrawList* fg = ImGui::GetForegroundDrawList();
        const int before = fg->VtxBuffer.Size;
        BuildSilhouettes(fg, base, localPawn, localTeam, mat, ds);
        if (fg->VtxBuffer.Size == before)
            return;

        ImDrawData dd;
        dd.Clear();
        dd.DisplayPos = ImVec2(0.0f, 0.0f);
        dd.DisplaySize = ImVec2(ds.x, ds.y);
        dd.FramebufferScale = ImVec2(1.0f, 1.0f);
        dd.AddDrawList(fg);
        dd.Valid = true;

        ID3D11RenderTargetView* prevRtv = nullptr;
        ID3D11DepthStencilView* prevDsv = nullptr;
        context->OMGetRenderTargets(1, &prevRtv, &prevDsv);

        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        context->ClearRenderTargetView(g_rtv, clear);
        context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(&dd);

        context->OMSetRenderTargets(1, &prevRtv, prevDsv);
        if (prevRtv) prevRtv->Release();
        if (prevDsv) prevDsv->Release();

        Bloom::BloomParams params;
        params.threshold = Config::kBloomThreshold;
        params.intensity = Config::kBloomIntensity;
        params.blurRadius = Config::kBloomBlurRadius;
        params.downscale = Config::kBloomDownscale;

        const Bloom::BloomColor color(Config::kBloomColorR, Config::kBloomColorG, Config::kBloomColorB, 1.0f);
        if (!g_bloom->Render(g_srv, color, params))
            return;

        ID3D11ShaderResourceView* out = g_bloom->GetOutputTexture();
        if (!out)
            return;

        ImGui::GetBackgroundDrawList()->AddImage(
            static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(out)),
            ImVec2(0.0f, 0.0f), ImVec2(ds.x, ds.y));
    }
}
