#include "nemesis_ui.h"

#include "../RenderDrx11/RenderHook.hpp"
#include "../resources.h"
#include "../resource_loader.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_extensions.h"
#include "layout_utils.h"

#include <vector>
#include <winuser.h>

namespace Nemesis::UI
{
	ImU32		 AccentColor = ImColor(230, 25, 52, 255);
	ImU32		 AccentColorSub = ImColor(250, 82, 101, 255);
	ImU32		 BgFillColor = ImColor(23, 23, 23, 255);
	ImU32		 BgFillColorSub = ImColor(31, 30, 30, 255);

	ImU32		 ElementFillColor = ImColor(35, 35, 35, 255);

	ImFont*		 MainFont = nullptr;

	float		 TotalWidth = 750.0f;
	float		 PanelWidth = 65.0f;
	const char*  UpperTitleStr = nullptr;

	float		 WidgetPadding = 4.0f;
	float		 MenuLayoutPadding = 10.0f;

	float		 Rounding = 10.0f;
	float		 WindowRounding = 15.0f;

	int			 CurrentPage = 0;

    ImVec2       WindowSize = ImVec2(700.0f, 500.0f);
	ImVec2		 LastRegionSize = ImVec2(0.0f, 0.0f);

	bool		 TEx = false;

	ID3D11ShaderResourceView* IconAim = nullptr;
	ID3D11ShaderResourceView* IconAimBot = nullptr;
	ID3D11ShaderResourceView* IconVisuals = nullptr;
	ID3D11ShaderResourceView* IconSettings = nullptr;
	ID3D11ShaderResourceView* IconSettingsAlt = nullptr;
	ID3D11ShaderResourceView* IconMovement = nullptr;
	ID3D11ShaderResourceView* IconVision = nullptr;
	ID3D11ShaderResourceView* TitleIcon = nullptr;

	void DrawMenu()
	{
		ImGuiStyle& g = ImGui::GetStyle();
		g.WindowRounding = WindowRounding;
		LastRegionSize = ImGui::GetContentRegionAvail();
		ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	    ImGui::SetNextWindowSize(WindowSize);

	    int screenW = GetSystemMetrics(SM_CXSCREEN);
	    int screenH = GetSystemMetrics(SM_CYSCREEN);
	    int winX = (screenW - WindowSize.x) / 2;
	    int winY = (screenH - WindowSize.y) / 2;
	    ImGui::SetNextWindowPos(ImVec2(winX, winY));

		ImGui::Begin("##Nemesis", nullptr, main_flags);
	    ImGui::PushFont(MainFont);

		DrawBoxShaded(ImGui::GetWindowPos(), ImGui::GetWindowSize(), BgFillColor, WindowRounding);
		DrawControlPanel();

	    ImGui::PopFont();
		ImGui::End();
	}

	void DrawControlPanel()
	{
	    ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 size(PanelWidth, ImGui::GetContentRegionAvail().y);

	    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::BeginChild("##NemesisControlPanel", size);
	    ImGui::PopStyleVar();

	    {
	        dl->PushClipRectFullScreen();
	        ImDrawFlags rounding_flags = ImDrawFlags_RoundCornersLeft;
	        DrawBoxShaded(ImGui::GetWindowPos(), size, BgFillColorSub, WindowRounding, rounding_flags);
	        dl->PopClipRect();

	        const float line_height = ImGui::GetTextLineHeight();
	        const float icon_side = line_height * 1.5f;
	        const ImVec2 icon_size = ImVec2(icon_side, icon_side);

	        const float button_width = PanelWidth - (WidgetPadding * 2.0f);
	        const ImVec2 button_size = ImVec2(button_width, button_width);

	        const ImVec2 button_spacer = ImVec2(0.0f, icon_side * 0.85f);

	        PushButtonStyle();
	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconAim, "##Aim", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 0), 0, button_size))
	        {
	            CurrentPage = 0;
	        }

	        ImGui::Dummy(button_spacer);

	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconAimBot, "##Aim Bot", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 1), 0, button_size))
	        {
	            CurrentPage = 1;
	        }

	        ImGui::Dummy(button_spacer);

	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconVisuals, "##Visuals", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 2), 0, button_size))
	        {
	            CurrentPage = 2;
	        }

	        ImGui::Dummy(button_spacer);

	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconMovement, "##Movement", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 3), 0, button_size))
	        {
	            CurrentPage = 3;
	        }

	        ImGui::Dummy(button_spacer);

	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconVision, "##Vision", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 4), 0, button_size))
	        {
	            CurrentPage = 4;
	        }

	        ImGui::Dummy(button_spacer);

	        ImGui::SetCursorPosX(WidgetPadding);
	        if (ImGuiExt::IconButton((ImTextureID)IconSettings, "##Settings", icon_size,
                AccentColor, AccentColorSub, (CurrentPage == 5), 0, button_size))
	        {
	            CurrentPage = 5;
	        }

	        ImGui::Dummy(button_spacer);

	        PopButtonStyle();
	    }

		ImGui::EndChild();
	}

	void DrawWidgetsStack();
	void DrawWidget(std::string title, const ImVec2 pos, std::function<void()> content);

	void DrawAimPage();
	void DrawVisualsPage();
	void DrawMovementPage();
	void DrawConfigsPage();
	void DrawDeveloperPage();
	void DrawSettingsPage();

	void PushButtonStyle()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Rounding);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(WidgetPadding, WidgetPadding));
		ImGui::PushStyleColor(ImGuiCol_Button, ElementFillColor);
	}

	void PopButtonStyle()
	{
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}

	void CreateResources()
	{
		ImGuiIO& io = ImGui::GetIO();
		ID3D11Device* g_pd3dDevice = Nemesis::RenderHook::GetDevice();

		std::vector<uint8_t> font_data;
		if (LoadResourceToMemory(IDR_FONT_INTER, font_data))
		{
			void* copy = IM_ALLOC(font_data.size());
			memcpy(copy, font_data.data(), font_data.size());

			ImFontConfig cfg;
			cfg.FontDataOwnedByAtlas = true;

			MainFont = io.Fonts->AddFontFromMemoryTTF(
				copy, (int)font_data.size(),
				16.0f, &cfg,
				io.Fonts->GetGlyphRangesCyrillic()
			);

			ImGui_ImplDX11_CreateDeviceObjects();
		}

		LoadTextureFromResource(IDR_ICON_AIM, g_pd3dDevice, &IconAim);
		LoadTextureFromResource(IDR_ICON_AIMBOT, g_pd3dDevice, &IconAimBot);
		LoadTextureFromResource(IDR_ICON_VISUALS, g_pd3dDevice, &IconVisuals);
		LoadTextureFromResource(IDR_ICON_SETTINGS, g_pd3dDevice, &IconSettings);
		LoadTextureFromResource(IDR_ICON_SETTINGS_ALT, g_pd3dDevice, &IconSettingsAlt);
		LoadTextureFromResource(IDR_ICON_MOVEMENT, g_pd3dDevice, &IconMovement);
		LoadTextureFromResource(IDR_ICON_VISION, g_pd3dDevice, &IconVision);
	}
}