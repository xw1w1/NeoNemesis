#include "RenderHook.hpp"
#include "Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"
#include "Miscellaneous Functions/L functions/Esp/Esp.hpp"
#include "Miscellaneous Functions/L functions/LegitBot/LegitBot.hpp"
#include "Miscellaneous Functions/R Functions/NemsisProject/RageBot.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "kiero.hpp"
#include "kiero_d3d11.hpp"

#include "../ImGuiUI/nemesis_ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Nemesis::RenderHook
{
    namespace
    {
        using PresentFn = HRESULT (__stdcall*)(IDXGISwapChain*, UINT, UINT);

        PresentFn               g_origPresent = nullptr;
        ID3D11Device*           g_device = nullptr;
        ID3D11DeviceContext*    g_context = nullptr;
        ID3D11RenderTargetView* g_rtv = nullptr;
        HWND                    g_window = nullptr;
        WNDPROC                 g_origWndProc = nullptr;
        void*                   g_present = nullptr;
        bool                    g_initialized = false;
        bool                    g_menuOpen = false;

        LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            if (msg == WM_KEYDOWN && wParam == VK_INSERT)
                g_menuOpen = !g_menuOpen;

            if (g_menuOpen)
            {
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

                if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) ||
                    (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) ||
                    msg == WM_INPUT || msg == WM_SETCURSOR)
                    return TRUE;
            }

            return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
        }

        bool Initialize(IDXGISwapChain* swapChain)
        {
            if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device))))
                return false;

            g_device->GetImmediateContext(&g_context);

            DXGI_SWAP_CHAIN_DESC desc{};
            swapChain->GetDesc(&desc);
            g_window = desc.OutputWindow;

            ID3D11Texture2D* backBuffer = nullptr;
            if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) && backBuffer)
            {
                g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
                backBuffer->Release();
            }

            ImGui::CreateContext();
            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(g_window);
            ImGui_ImplDX11_Init(g_device, g_context);

            g_origWndProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

            NLOG("RenderHook: ImGui DX11 initialized");
            return true;
        }

        HRESULT __stdcall HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
        {
            if (!g_initialized)
            {
                if (!Initialize(swapChain))
                    return g_origPresent(swapChain, syncInterval, flags);
                g_initialized = true;
            }

            if (g_rtv)
            {
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                Nemesis::Esp::Render();
                Nemesis::RageBot::Render(); 
                // Nemesis::LegitBot::Render(); //

                if (g_menuOpen)
                {
                    ImGui::GetIO().MouseDrawCursor = true;
                    Nemesis::UI::DrawMenu();
                } 
                else
                {
                    ImGui::GetIO().MouseDrawCursor = false;
                }

                ImGui::Render();
                g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            }

            return g_origPresent(swapChain, syncInterval, flags);
        }
    }

    void Start()
    {
        kiero::D3D11Output output;
        if (kiero::locate<kiero::Implementation_D3D11>(nullptr, &output) != kiero::Error_Nil)
        {
            NWARN("RenderHook: D3D11 locate failed");
            return;
        }

        if (output.swapchain_methods.size() <= 8)
        {
            NWARN("RenderHook: swapchain methods missing");
            return;
        }

        g_present = output.swapchain_methods[8];

        MH_Initialize();
        if (MH_CreateHook(g_present, reinterpret_cast<void*>(&HookPresent),
                          reinterpret_cast<void**>(&g_origPresent)) == MH_OK)
        {
            MH_EnableHook(g_present);
            NLOG("RenderHook: Present hooked");
        }
    }

    void Stop()
    {
        if (g_present)
            MH_DisableHook(g_present);

        if (g_initialized && g_window && g_origWndProc)
            SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));
    }
}
