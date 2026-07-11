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

#include "../Miscellaneous Functions/AimConfig.h"

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

    void DrawContentPane();

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
        DrawContentPane();

		ImGui::PopFont();
		ImGui::End();
	}

    void DrawContentPane()
    {
        const float content_x = PanelWidth;
        const float content_w = WindowSize.x - PanelWidth;
        const float content_h = WindowSize.y;

        ImGui::SetCursorPos(ImVec2(content_x, 0.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild(
            "##NemesisContent",
            ImVec2(content_w, content_h),
            false,
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse
        );
        ImGui::PopStyleVar();

        switch (CurrentPage)
        {
        case 0: DrawAimPage(); break;
        case 1: /* DrawConfigsPage(); */ break;
        case 2: /* DrawVisualsPage(); */ break;
        case 3: /* DrawMovementPage(); */ break;
        case 4: /* DrawDeveloperPage(); */ break;
        case 5: /* DrawSettingsPage(); */ break;
        default: break;
        }

        ImGui::EndChild();
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

        const float button_gap = 4.5f;
        const float edge_padding = WidgetPadding * 8.0f;

        const float button_side = PanelWidth - (WidgetPadding * 4.2f);
        const ImVec2 button_size(button_side, button_side);

        const float icon_side = button_side * 0.5f;
        const ImVec2 icon_size(icon_side, icon_side);

        float start_x = (size.x - button_size.x) * 0.5f;
        if (start_x < 0.0f)
            start_x = 0.0f;

        const float title_icon_padding = start_x;
        const float title_icon_size = PanelWidth - title_icon_padding * 2.0f;
        const float title_icon_x = title_icon_padding;
        const float title_icon_y = title_icon_padding;

        if (TitleIcon)
        {
            ImGui::SetCursorPos(ImVec2(title_icon_x, title_icon_y));
            ImGui::Image(
                (ImTextureID)TitleIcon,
                ImVec2(title_icon_size, title_icon_size)
            );
        }

        const float icon_to_buttons_gap = button_side * 0.65f;
        float top_y = title_icon_y + title_icon_size + icon_to_buttons_gap;

        PushButtonStyle();

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

    void BeginFeatureBox(const char* title, float width, float height)
    {
        ImGui::BeginGroup();
        ImGui::PushID(title);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(width, height);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float rounding = 8.0f;
        ImU32 bg = IM_COL32(30, 30, 32, 255);
        ImU32 border = IM_COL32(50, 50, 52, 255);

        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg, rounding);
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), border, rounding, 0, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
        ImGui::BeginChild(
            title,
            size,
            false,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
        );

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }

    void EndFeatureBox()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopID();
        ImGui::EndGroup();
    }

    void DrawAimPage()
    {
        const float page_padding = 24.0f;
        const float box_gap = 16.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(page_padding, page_padding));
        ImGui::BeginChild(
            "##AimPageScroll",
            ImVec2(0, 0),
            ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoBackground
        );
        ImGui::PopStyleVar();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::TextUnformatted("Aim Assistance");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        float content_width = ImGui::GetContentRegionAvail().x;

        float box_height = 220.0f;
        float box_width = (content_width - box_gap) * 0.5f;

        PushButtonStyle();

        ImGui::BeginGroup();
        {
            BeginFeatureBox("Aim Bot", box_width, box_height);

            SettingRow("Enabled", 40.0f, []() {
                ImGuiExt::Toggle(
                    "##aimbot_toggle",
                    &g_AimConfig.aimbotEnabled, // bool ВКЛЮЧЕН ЛИ АИМБОТ
                    AccentColor,
                    AccentColor,
                    ImVec2(40.0f, 20.0f)
                );
                });

            SettingRow("FOV", 120.0f, []() {
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, AccentColor);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(45, 45, 48, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::SliderFloat(
                    "##aimbot_fov",
                    &g_AimConfig.aimbotFov,   // float ФОВ АИМБОТА
                    0.5f, 30.0f, "%.1f"
                );
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
                });

            SettingRow("Smooth", 120.0f, []() {
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, AccentColor);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(45, 45, 48, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::SliderFloat(
                    "##aimbot_smooth",
                    &g_AimConfig.aimbotSmooth, // float СГЛАЖИВАНИЕ АИМБОТА
                    1.0f, 20.0f, "%.1f"
                );
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
                });

            SettingRow("Vis Check", 40.0f, []() {
                ImGuiExt::Toggle(
                    "##aimbot_vischeck",
                    &g_AimConfig.aimbotVisCheck, // визуальнный чек
                    AccentColor,
                    AccentColor,
                    ImVec2(40.0f, 20.0f)
                );
                });

            EndFeatureBox();
        }
        ImGui::EndGroup();

        ImGui::SameLine(0.0f, box_gap);

        ImGui::BeginGroup();
        {
            BeginFeatureBox("Rage Bot", box_width, box_height);

            SettingRow("Enabled", 40.0f, []() {
                ImGuiExt::Toggle(
                    "##triggerbot_toggle",
                    &g_AimConfig.rageMode,   //bool триггер бот
                    AccentColor,
                    AccentColor,
                    ImVec2(40.0f, 20.0f)
                );
                });

            SettingRow("Delay (ms)", 120.0f, []() {
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, AccentColor);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(45, 45, 48, 255));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::SliderFloat(
                    "##triggerbot_delay",
                    &g_AimConfig.triggerbotDelay, // float задержка триггербота
                    0.0f, 300.0f, "%.0f"
                );
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
                });

            SettingRow("Head Only", 40.0f, []() {
                ImGuiExt::Toggle(
                    "##triggerbot_headonly",
                    &g_AimConfig.triggerbotHeadOnly, // триггербот только в голову
                    AccentColor,
                    AccentColor,
                    ImVec2(40.0f, 20.0f)
                );
                });

            SettingRow("Scope Only", 40.0f, []() {
                ImGuiExt::Toggle(
                    "##triggerbot_scopeonly",  // триггербот только с прицелом
                    &g_AimConfig.triggerbotScopeOnly,
                    AccentColor,
                    AccentColor,
                    ImVec2(40.0f, 20.0f)
                );
                });

            EndFeatureBox();
        }
        ImGui::EndGroup();
        PopButtonStyle();

        ImGui::EndChild();
    }

    void DrawVisualsPage()
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextUnformatted(" coming soon");
    }

    void DrawMovementPage()
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextUnformatted("coming soon");
    }

    void DrawConfigsPage()
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextUnformatted("coming soon");
    }

    void DrawDeveloperPage()
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextUnformatted("coming soon");
    }

    void DrawSettingsPage()
    {
        ImGui::SetCursorPos(ImVec2(24.0f, 24.0f));
        ImGui::TextUnformatted("coming soon");
    }

	void PushButtonStyle()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Rounding);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(WidgetPadding, WidgetPadding));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentColorSub);
	}

	void PopButtonStyle()
	{
		ImGui::PopStyleColor(2);
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
		LoadTextureByName("uglycaticon.png", g_pd3dDevice, &TitleIcon);
	}
}