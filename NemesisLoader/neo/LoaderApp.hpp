#pragma once

#include <windows.h>
#include <d3d11.h>
#include "imgui.h"

#include "../system_info.h"

namespace Nemesis::LoaderApp
{
	extern float DPI;
	extern ImVec2 WINDOW_SIZE;

	extern HWND hwnd;

	extern ImVec4 NCHITTEST_PTS;
	extern ImVec4 DX_RT_CLEARCOLOR;

	extern ID3D11Device* device;
	extern ID3D11DeviceContext* deviceContext;

	extern IDXGISwapChain* swapChain;
	extern ID3D11RenderTargetView* renderTargetView;

	extern ID3D11Texture2D* sceneCaptureTex;
	extern ID3D11ShaderResourceView* sceneCaptureSrv;

	extern SystemInfo systemInfo;

	extern bool ShouldClose;
	extern bool BlockHeaderCommands;

	bool LoaderInit(WNDCLASSEXW wc, bool coInitialized);
	void CreateResources();

	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();

	void CreateRenderTarget();
	void CleanupRenderTarget();

	void UpdateSceneCapture();
	bool EnsureSceneCapture(UINT w, UINT h);
	void CleanupSceneCapture();

	SystemInfo GetSpecs();
}