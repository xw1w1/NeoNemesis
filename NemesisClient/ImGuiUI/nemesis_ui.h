#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <d3d11.h>

#include <functional>
#include <string>

namespace Nemesis::UI
{
	void DrawMenu();
	void DrawControlPanel();
	void DrawWidgetsStack();
	void DrawWidget(std::string title, const ImVec2 pos, std::function<void()> content);

	void DrawAimPage();
	void DrawVisualsPage();
	void DrawMovementPage();
	void DrawConfigsPage();
	void DrawDeveloperPage();
	void DrawSettingsPage();

	void PushButtonStyle();
	void PopButtonStyle();
	
	void CreateResources();
}