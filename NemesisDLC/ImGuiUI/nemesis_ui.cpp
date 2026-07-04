#include "nemesis_ui.h"
#include "imgui_extensions.h"
#include "layout_utils.h"

namespace Nemesis::UI
{
	ImU32		 AccentColor = ImColor(230, 25, 52, 255);
	ImU32		 AccentColorSub = ImColor(250, 82, 101, 255);
	ImU32		 BgFillColor = ImColor(23, 23, 23, 255);
	ImU32		 BgFillColorSub = ImColor(31, 30, 30, 255);

	ImU32		 ElementFillColor = ImColor(35, 35, 35, 255);

	float		 TotalWidth = 750.0f;
	float		 PanelWidth = 75.0f;
	float		 SubPanelWidth = 160.0f;
	const char*  UpperTitleStr = nullptr;

	float		 WidgetPadding = 4.0f;
	float		 MenuLayoutPadding = 10.0f;

	float		 Rounding = 10.0f;
	float		 WindowRounding = 15.0f;

	ID3D11ShaderResourceView* TitleIcon = nullptr;

	int			 CurrentPage = 0;

	ImVec2		 LastRegionSize = ImVec2(0.0f, 0.0f);

	bool		 TEx = false;

	void DrawMenu()
	{
		ImGuiStyle& g = ImGui::GetStyle();
		g.WindowRounding = WindowRounding;
		LastRegionSize = ImGui::GetContentRegionAvail();
		ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		ImGui::Begin("##Nemesis", nullptr, main_flags);
		DrawBoxShaded(ImGui::GetWindowPos(), ImGui::GetWindowSize(), BgFillColor, WindowRounding);
		DrawControlPanel();
		ImGui::End();
	}

	void DrawControlPanel()
	{
		ImVec2 size(PanelWidth, ImGui::GetContentRegionAvail().y);
		ImGui::BeginChild("##NemesisControlPanel", size);
		DrawBoxShaded(ImVec2(0.0f, 0.0f), size, BgFillColorSub, WindowRounding);

		PushButtonStyle();
		ImGuiExt::Toggle("##toggle", &TEx, AccentColor, AccentColorSub);

		ImGuiExt::Button("Button", AccentColor, AccentColorSub);
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
		ImGui::PushStyleColor(ImGuiCol_Button, ElementFillColor);
	}

	void PopButtonStyle()
	{
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}
}