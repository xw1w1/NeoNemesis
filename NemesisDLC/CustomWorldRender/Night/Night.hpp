#pragma once

namespace Nemesis::Night
{
    // Вызывать из HookPresent после ImGui::NewFrame(), ДО ESP (сцена темнеет, ESP/меню поверх).
    void Render();
}
