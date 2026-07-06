#pragma once

#include <d3d11.h>

struct ImFont;

namespace Nemesis::RenderHook
{
    void Start();
    void Stop();
    ImFont* GetEspFont();
    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();
}
