#pragma once

struct ImFont;

namespace Nemesis::RenderHook
{
    void Start();
    void Stop();
    ImFont* GetEspFont();
}
