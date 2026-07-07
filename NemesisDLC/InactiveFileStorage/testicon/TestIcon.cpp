#include "TestIcon.hpp"

#include <Windows.h>
#include "imgui.h"
#include "../../RenderDrx11/RenderHook.hpp"
#include "../../resource_loader.h"

namespace Nemesis::TestIcon
{
    namespace
    {
        ID3D11ShaderResourceView* g_srv = nullptr;
        bool g_loaded = false;
        bool g_show = false;
        bool g_prev = false;
        const ImVec2 kSize = ImVec2(256.0f, 256.0f);
    }

    void Render()
    {
        if (!Enabled)
            return;

        const bool key = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (key && !g_prev)
            g_show = !g_show;
        g_prev = key;

        if (!g_show)
            return;

        if (!g_loaded)
        {
            LoadTextureByName("toggle-on.png", Nemesis::RenderHook::GetDevice(), &g_srv);
            g_loaded = true;
        }

        if (!g_srv)
            return;

        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const ImVec2 c = ImVec2(ds.x * 0.5f, ds.y * 0.5f);
        const ImVec2 a = ImVec2(c.x - kSize.x * 0.5f, c.y - kSize.y * 0.5f);
        const ImVec2 b = ImVec2(a.x + kSize.x, a.y + kSize.y);
        ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)g_srv, a, b);
    }
}
