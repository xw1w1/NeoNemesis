#include "LoaderApp.hpp"
#include "LoaderUI.hpp"

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include <d3d11.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "../system_info.h"

#pragma comment(lib, "Shell32.lib")

namespace Nemesis::LoaderApp
{
	HWND					  hwnd = nullptr;

	ID3D11Device*			  device = nullptr;
	ID3D11DeviceContext*	  deviceContext = nullptr;

	IDXGISwapChain*			  swapChain = nullptr;
	ID3D11RenderTargetView*   renderTargetView = nullptr;

	ID3D11Texture2D*		  sceneCaptureTex = nullptr;
	ID3D11ShaderResourceView* sceneCaptureSrv = nullptr;

	float					  DPI = 1.0f;
	ImVec2					  WINDOW_SIZE = ImVec2(800, 520);
	ImVec4					  NCHITTEST_PTS = ImVec4(35, 8, 22, 6);
	ImVec4					  DX_RT_CLEARCOLOR = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);

	SystemInfo systemInfo;

	bool ShouldClose = false;
	bool BlockHeaderCommands = false;

	bool LoaderInit(WNDCLASSEXW wc, bool coInitialized)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGuiStyle& g = ImGui::GetStyle();
		g.AntiAliasedFill = true;
		g.AntiAliasedLines = true;
		g.ScaleAllSizes(DPI);
		g.FontScaleDpi = DPI;

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX11_Init(device, deviceContext);
		CreateResources();

		LoaderUI::Init();

		while (!ShouldClose)
		{
			MSG msg;
			while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				if (msg.message == WM_QUIT)
				{
					ShouldClose = true;
				}
			}
			if (ShouldClose)
			{
				break;
			}

			LoaderUI::DrawFrame();

			const float cc[4] = { DX_RT_CLEARCOLOR.x * DX_RT_CLEARCOLOR.w, DX_RT_CLEARCOLOR.y * DX_RT_CLEARCOLOR.w, DX_RT_CLEARCOLOR.z * DX_RT_CLEARCOLOR.w, DX_RT_CLEARCOLOR.w };
			deviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
			deviceContext->ClearRenderTargetView(renderTargetView, cc);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			swapChain->Present(1, 0);
		}
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CleanupDeviceD3D();
		::DestroyWindow(hwnd);
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		if (coInitialized)
		{
			CoUninitialize();
		}
		
		return 0;
	}

	void CreateResources()
	{
		systemInfo = SystemInfoCollector::Collect(device);

		ImGuiIO& io = ImGui::GetIO(); (void)io;
	}

	bool CreateDeviceD3D(HWND hWnd)
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &deviceContext);
		if (res == DXGI_ERROR_UNSUPPORTED)
			res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &swapChain, &device, &featureLevel, &deviceContext);
		if (res != S_OK)
			return false;

		CreateRenderTarget();
		return true;
	}

	void CleanupDeviceD3D()
	{
		CleanupRenderTarget();
		swapChain->Release();
		swapChain = nullptr;
		
		deviceContext->Release();
		deviceContext = nullptr;

		device->Release();
		device = nullptr;
	}

	void CreateRenderTarget()
	{
		ID3D11Texture2D* pBackBuffer;
		swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		device->CreateRenderTargetView(pBackBuffer, nullptr, &renderTargetView);
		pBackBuffer->Release();
	}

	void CleanupRenderTarget()
	{
		renderTargetView->Release();
		renderTargetView = nullptr;
	}

	SystemInfo GetSpecs()
	{
		return systemInfo;
	}
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (Nemesis::LoaderApp::device != nullptr && wParam != SIZE_MINIMIZED)
		{
			Nemesis::LoaderApp::CleanupRenderTarget();
			Nemesis::LoaderApp::swapChain->ResizeBuffers(
				0, LOWORD(lParam), HIWORD(lParam),
				DXGI_FORMAT_UNKNOWN, 0);
			Nemesis::LoaderApp::CreateRenderTarget();
		}
		return 0;

	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;

		return ::DefWindowProcW(hWnd, msg, wParam, lParam);

	case WM_NCHITTEST:
	{
		if (Nemesis::LoaderApp::BlockHeaderCommands)
		{
			return HTCLIENT;
		}

		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(hWnd, &pt);

		RECT rc{}; GetClientRect(hWnd, &rc);
		const int W = rc.right;  const int H = rc.bottom;

		const int TOP_BAR = Nemesis::LoaderApp::NCHITTEST_PTS.x;
		const int PAD = Nemesis::LoaderApp::NCHITTEST_PTS.y;
		const int BTN = Nemesis::LoaderApp::NCHITTEST_PTS.z;
		const int SP = Nemesis::LoaderApp::NCHITTEST_PTS.w;

		RECT rClose{ W - PAD - BTN, PAD, W - PAD, PAD + BTN };
		RECT rMin{ W - PAD - 2 * BTN - SP, PAD, W - PAD - BTN - SP, PAD + BTN };
		RECT rCBClose{ rClose.left + 2, rClose.bottom + 4, rClose.left + 2 + BTN - 4, rClose.bottom + 4 + BTN - 4 };
		RECT rCBMin{ rMin.left + 2, rMin.bottom + 4, rMin.left + 2 + BTN - 4, rMin.bottom + 4 + BTN - 4 };

		auto in_rect = [](POINT p, const RECT& r) -> bool {
			return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
			};

		if (in_rect(pt, rClose) || in_rect(pt, rMin) || in_rect(pt, rCBClose) || in_rect(pt, rCBMin))
			return HTCLIENT;

		if (pt.y < TOP_BAR)
			return HTCAPTION;

		return HTCLIENT;
	}
	}
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	using namespace Nemesis::LoaderApp;

	if (!IsUserAnAdmin())
	{
		MessageBoxW(NULL, L"Administrator rights are required to run this application.",
			L"Nemesis Loader", MB_ICONERROR);
		return 1;
	}

	HRESULT coInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	bool    coInitialized = SUCCEEDED(coInit);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpszClassName = L"Nemesis Loader";

	::RegisterClassExW(&wc);

	int screen_w, screen_h;
	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);

	int win_x, win_y;
	win_x = (screen_w - WINDOW_SIZE.x) / 2;
	win_y = (screen_h - WINDOW_SIZE.y) / 2;

	ImGui_ImplWin32_EnableDpiAwareness();
	DPI = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
	hwnd = ::CreateWindowW(wc.lpszClassName, L"NemesisLoader", WS_POPUP, win_x, win_y, (int)(WINDOW_SIZE.x * DPI), (int)(WINDOW_SIZE.y * DPI), nullptr, nullptr, wc.hInstance, nullptr);

	const int radius = 16;
	const int diameter = radius * 2;

	HRGN region = CreateRoundRectRgn(0, 0, WINDOW_SIZE.x + 1, WINDOW_SIZE.y + 1, diameter, diameter);
	SetWindowRgn(hwnd, region, TRUE);

	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	LoaderInit(wc, coInitialized);
	return 0;
}