#include "LoaderApp.hpp"

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>

#include <d3d11.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "system_info.h"

#pragma comment(lib, "Shell32.lib")

float  DPI = 1.0f;
ImVec2 windowSizeDef = ImVec2(800, 520);

namespace Nemesis::LoaderApp
{
	HWND hwnd = nullptr;
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;

	IDXGISwapChain* swapChain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	ID3D11Texture2D* sceneCaptureTex = nullptr;
	ID3D11ShaderResourceView* sceneCaptureSrv = nullptr;

	SystemInfo systemInfo = nullptr;

	bool ShouldClose = false;

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

		while (!ShouldClose)
		{
			MSG msg;
			while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				if (msg == WM_QUIT)
				{
					ShouldClose = true;
				}
			}
			if (ShouldClose)
			{
				break;
			}

			// Поскольку это главный цикл, здесь должны происходить все апдейты.

			//LoaderUI::DrawFrame();

			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			swapChain->Present(1, 0);
		}
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		//Shutdown всех остальных мажорных сервисов лоадера
		CleanupDeviceD3D();
		::DestroyWindow(hwnd);
		::UnregisterClassA(wc.lpszClassName, wc.hInstance);
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
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

	::RegisterClassExW(wc);

	int screen_w, screen_h;
	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);

	int win_x, win_y;
	win_x = (screen_w - windowSizeDef.x) / 2;
	win_y = (screen_h - windowSizeDef.y) / 2;

	ImGui_ImplWin32_EnableDpiAwareness();
	DPI = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
	hwnd = ::CreateWindowW(wc.lpszClassName, L"NemesisLoader", WS_POPUP, win_x, win_y, (int)(windowSizeDef.x * scale), (int)(windowSizeDef.y * scale), nullptr, nullptr, wc.hInstance, nullptr);

	const int radius = 16;
	const int diameter = radius * 2;

	HRGN region = CreateRoundRectRgn(0, 0, windowSizeDef.x + 1, windowSizeDef.y + 1, diameter, diameter);
	SetWindowRgn(hwnd, region, TRUE);

	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);
	if (!LoaderInit(wc, coInitialized))
	{
		MessageBoxW(NULL, L"Unable to startup Loader. Please contact developers.",
			L"Nemesis Loader", MB_ICONERROR);
		return 1;
	}
}