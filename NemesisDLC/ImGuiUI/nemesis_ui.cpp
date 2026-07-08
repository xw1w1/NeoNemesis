#include "nemesis_ui.h"

#include "../RenderDrx11/RenderHook.hpp"
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
	ImU32		 AccentColorSub = ImColor(38, 38, 38, 255);
	ImU32		 BgFillColor = ImColor(23, 23, 23, 255);
	ImU32		 BgFillColorSub = ImColor(31, 30, 30, 255);

	ImU32		 ElementFillColor = ImColor(35, 35, 35, 255);

	ImFont*		 MainFont = nullptr;

	float		 TotalWidth = 750.0f;
	float		 PanelWidth = 60.0f;
	const char*  UpperTitleStr = nullptr;

	float		 WidgetPadding = 4.0f;
	float		 MenuLayoutPadding = 10.0f;

	float		 Rounding = 10.0f;
	float		 WindowRounding = 15.0f;

	int			 CurrentPage = 0;

    ImVec2       WindowSize = ImVec2(835.0f, 520.0f);
	ImVec2		 LastRegionSize = ImVec2(0.0f, 0.0f);
	ImVec2		 LastKnownRegionPos = ImVec2(0.0f, 0.0f);

	bool		 TEx = false;

	ID3D11ShaderResourceView* IconAim = nullptr;
	ID3D11ShaderResourceView* IconCloud = nullptr;
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

		ImGuiWindowFlags main_flags =
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::SetNextWindowSize(WindowSize);

		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int screenH = GetSystemMetrics(SM_CYSCREEN);
		int winX = (screenW - (int)WindowSize.x) / 2;
		int winY = (screenH - (int)WindowSize.y) / 2;
		ImGui::SetNextWindowPos(ImVec2((float)winX, (float)winY));

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("##Nemesis", nullptr, main_flags);
		ImGui::PopStyleVar();

		ImGui::PushFont(MainFont);

		DrawBoxShaded(
			ImGui::GetWindowPos(),
			ImGui::GetWindowSize(),
			BgFillColor,
			WindowRounding,
			true,
			ImDrawFlags_RoundCornersAll
		);

		DrawControlPanel();

		ImGui::PopFont();
		ImGui::End();
	}

	void DrawControlPanel()
	{
		ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

		ImVec2 panel_pos = ImGui::GetCursorScreenPos();
		ImVec2 size(PanelWidth, ImGui::GetWindowHeight());

		ImGui::PushStyleColor(ImGuiCol_Border, AccentColorSub);
		DrawBoxShaded(
			panel_pos,
			size,
			BgFillColorSub,
			WindowRounding,
			true,
			ImDrawFlags_RoundCornersLeft
		);
		ImGui::PopStyleColor();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::BeginChild(
			"##NemesisControlPanel",
			size,
			false,
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse
		);
		ImGui::PopStyleVar();

		struct PanelButton
		{
			ID3D11ShaderResourceView* icon;
			const char* id;
			int page_index;
		};

		PanelButton top_buttons[] =
		{
			{ IconAim,      "##Aim",      0 },
			{ IconVisuals,  "##Visuals",  2 },
			{ IconMovement, "##Movement", 3 },
			{ IconVision,   "##Vision",   4 },
		};

		PanelButton bottom_buttons[] =
		{
			{ IconCloud,    "##Configs",  1 },
			{ IconSettings, "##Settings", 5 },
		};

		const int top_count = (int)(sizeof(top_buttons) / sizeof(top_buttons[0]));
		const int bottom_count = (int)(sizeof(bottom_buttons) / sizeof(bottom_buttons[0]));

		const float button_gap = 8.0f;
		const float edge_padding = WidgetPadding * 8.0f;

		const float button_side = PanelWidth - (WidgetPadding * 4.2f);
		const ImVec2 button_size(button_side, button_side);

		const float icon_side = button_side * 0.5f;
		const ImVec2 icon_size(icon_side, icon_side);

		float start_x = (size.x - button_size.x) * 0.5f;
		if (start_x < 0.0f)
			start_x = 0.0f;

		PushButtonStyle();

		float top_y = WidgetPadding * 16.0f;
		ImGui::SetCursorPos(ImVec2(start_x, top_y));

		for (int i = 0; i < top_count; ++i)
		{
			ImGui::SetCursorPosX(start_x);

			if (ImGuiExt::IconButton(
				(ImTextureID)top_buttons[i].icon,
				top_buttons[i].id,
				icon_size,
				AccentColor, AccentColorSub,
				(CurrentPage == top_buttons[i].page_index),
				0, button_size))
			{
				CurrentPage = top_buttons[i].page_index;
			}

			if (i < top_count - 1)
			{
				ImGui::SetCursorPosX(start_x);
				ImGui::Dummy(ImVec2(0.0f, button_gap));
			}
		}

		float bottom_total_height = bottom_count * button_size.y + (bottom_count - 1) * button_gap;
		float bottom_y = size.y - edge_padding - bottom_total_height;

		ImGui::SetCursorPos(ImVec2(start_x, bottom_y));

		for (int i = 0; i < bottom_count; ++i)
		{
			ImGui::SetCursorPosX(start_x);

			if (ImGuiExt::IconButton(
				(ImTextureID)bottom_buttons[i].icon,
				bottom_buttons[i].id,
				icon_size,
				AccentColor, AccentColorSub,
				(CurrentPage == bottom_buttons[i].page_index),
				0, button_size))
			{
				CurrentPage = bottom_buttons[i].page_index;
			}

			if (i < bottom_count - 1)
			{
				ImGui::SetCursorPosX(start_x);
				ImGui::Dummy(ImVec2(0.0f, button_gap));
			}
		}

		PopButtonStyle();
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
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
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

		unsigned int font_size = 0;
		const unsigned char* font_bytes = GetResourceBytes("Inter_18pt-Medium.ttf", &font_size);
		if (font_bytes && font_size)
		{
			void* copy = IM_ALLOC(font_size);
			memcpy(copy, font_bytes, font_size);

			ImFontConfig cfg;
			cfg.FontDataOwnedByAtlas = true;

			MainFont = io.Fonts->AddFontFromMemoryTTF(
				copy, (int)font_size,
				16.0f, &cfg,
				io.Fonts->GetGlyphRangesCyrillic()
			);

			ImGui_ImplDX11_CreateDeviceObjects();
		}

		LoadTextureByName("target.png", g_pd3dDevice, &IconAim);
		LoadTextureByName("cloud.png", g_pd3dDevice, &IconCloud);
		LoadTextureByName("sparkles.png", g_pd3dDevice, &IconVisuals);
		LoadTextureByName("settings.png", g_pd3dDevice, &IconSettings);
		LoadTextureByName("settings-sliders.png", g_pd3dDevice, &IconSettingsAlt);
		LoadTextureByName("running.png", g_pd3dDevice, &IconMovement);
		LoadTextureByName("eye.png", g_pd3dDevice, &IconVision);
	}
}