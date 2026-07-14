#include "LoaderUI.hpp"
#include "LoaderApp.hpp"

#include <d3d11.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

namespace Nemesis::LoaderUI
{
	void Init()
	{

	}

	void DrawFrame()
	{
		using namespace Nemesis::LoaderApp;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(WINDOW_SIZE);

			ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (BlockHeaderCommands) main_flags |= ImGuiWindowFlags_NoInputs;

			ImGui::Begin("Nemesis Loader", nullptr, main_flags);

			DrawBackgound();

			ImGui::End();
		}
		ImGui::Render();
	}

	void DrawControls()
	{

	}

	void DrawBackgound()
	{

	}
}