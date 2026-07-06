#include "Night.hpp"
#include "../../AllUsedAddresses/Address/AllUsedAddresses.hpp"
#include "../../Miscellaneous Utilities/LogsSystem/LogsSystem.hpp"

#include "imgui.h"

#include <atomic>
#include <Windows.h>

namespace Nemesis::Night
{
    using namespace Nemesis::Addresses;

    namespace
    {
        std::atomic<bool> g_on{ false };
    }

    void Render()
    {
        static bool prev = false;
        const bool key = (GetAsyncKeyState(NightMode::kToggleKey) & 0x8000) != 0;
        if (key && !prev)
        {
            g_on.store(!g_on.load());
            NLOG("[night] %s", g_on.load() ? "ON" : "OFF");
        }
        prev = key;

        if (!g_on.load())
            return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        const ImVec2 sz = ImGui::GetIO().DisplaySize;
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), sz,
                          IM_COL32(NightMode::kTintR, NightMode::kTintG, NightMode::kTintB, NightMode::kTintA));
    }
}
