#pragma once

#include "imgui.h"
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace NemesisBlur
{
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    void Shutdown();

    void CaptureBackBuffer(IDXGISwapChain* swapChain);

    void DrawBlurredBackBufferRect(
        ImDrawList* drawList,
        const ImVec2& min,
        const ImVec2& max,
        float rounding = 0.0f,
        ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
        ImU32 tint = ImColor(255, 255, 255, 255),
        int passes = 1,
        float blurRadius = 1.0f,
        bool dirty = false
    );

    ID3D11ShaderResourceView* GetBlurredTexture(
        const char* cacheKey,
        ID3D11ShaderResourceView* sourceSrv,
        UINT dstWidth,
        UINT dstHeight,
        int passes,
        float blurRadius,
        bool dirty
    );

    ID3D11ShaderResourceView* GetBlurredBackBuffer(
        int passes,
        float blurRadius,
        bool dirty
    );

    void InvalidateAll();
}