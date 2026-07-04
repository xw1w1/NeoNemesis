#include "layout_utils.h"
#include "imgui_extensions.h"
#include <string>
#include <functional>

void SettingsSectionHeader(const char* title)
{
    ImGui::Dummy(ImVec2(0, 8.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 230, 230, 255));
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
    ImGui::Text("%s", title);
    ImGui::PopFont();
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p_min = ImGui::GetCursorScreenPos();
    ImVec2 p_max = ImVec2(p_min.x + ImGui::GetContentRegionAvail().x, p_min.y + 1.0f);
    dl->AddRectFilled(p_min, p_max, IM_COL32(60, 60, 60, 255));

    ImGui::Dummy(ImVec2(0, 12.0f));
}

void SettingsRow(const char* label, const char* tooltip)
{
    const float label_column_width = 200.0f;

    ImGui::Dummy(ImVec2(4.0f, 0));
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    if (tooltip && ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300.0f);
        ImGui::TextUnformatted(tooltip);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::SameLine(label_column_width);
    ImGui::SetNextItemWidth(200.0f);
}

void SettingsSliderInt(const char* label, int* value, int min, int max, const char* tooltip)
{
    SettingsRow(label, tooltip);
    std::string id = "##" + std::string(label);
    ImGui::SliderInt(id.c_str(), value, min, max);
}

void SettingsSliderFloat(const char* label, float* value, float min, float max, const char* format, const char* tooltip)
{
    SettingsRow(label, tooltip);
    std::string id = "##" + std::string(label);
    ImGui::SliderFloat(id.c_str(), value, min, max, format);
}

void SettingsColorEditU32(const char* label, ImU32* col, const char* tooltip)
{
    SettingsRow(label, tooltip);
    ImVec4 color = ImGui::ColorConvertU32ToFloat4(*col);
    std::string id = "##" + std::string(label);
    if (ImGui::ColorEdit4(id.c_str(), (float*)&color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
    {
        *col = ImGui::ColorConvertFloat4ToU32(color);
    }
}

void SettingsColorEditVec4(const char* label, ImVec4* col, const char* tooltip)
{
    SettingsRow(label, tooltip);
    std::string id = "##" + std::string(label);
    ImGui::ColorEdit4(id.c_str(), (float*)col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
}

void LabelValueRowCopyable(const char* label, const char* value, bool copyable, const char* copy_value)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float row_height = 28.0f;

    ImGui::InvisibleButton((std::string("##row_") + label).c_str(), ImVec2(width, row_height));
    bool hovered = ImGui::IsItemHovered() && copyable;
    bool clicked = ImGui::IsItemClicked() && copyable;

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + row_height),
            IM_COL32(255, 255, 255, 8), 4.0f);
    }

    if (clicked && copy_value)
    {
        ImGui::SetClipboardText(copy_value);
    }

    ImFont* font = ImGui::GetFont();
    float font_size = ImGui::GetFontSize();
    float text_y = pos.y + (row_height - font_size) * 0.5f;

    dl->AddText(font, font_size,
        ImVec2(pos.x + 8.0f, text_y),
        IM_COL32(140, 145, 155, 255), label);

    ImVec2 value_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, value);
    float value_x = pos.x + width - value_size.x - 8.0f;

    if (copyable)
    {
        value_x -= 20.0f;
        float ico_x = pos.x + width - 20.0f;
        float ico_y = pos.y + row_height * 0.5f - 4.0f;
        ImU32 ico_col = hovered ? IM_COL32(200, 200, 210, 255) : IM_COL32(120, 125, 135, 255);
        dl->AddRect(ImVec2(ico_x, ico_y), ImVec2(ico_x + 8, ico_y + 10), ico_col, 1.5f, 0, 1.0f);
        dl->AddRect(ImVec2(ico_x + 3, ico_y - 3), ImVec2(ico_x + 11, ico_y + 7), ico_col, 1.5f, 0, 1.0f);
    }

    dl->AddText(font, font_size,
        ImVec2(value_x, text_y),
        IM_COL32(230, 230, 235, 255), value);

    if (hovered && copyable)
    {
        ImGui::BeginTooltip();
        ImGui::Text("Click to copy");
        ImGui::EndTooltip();
    }
}

void DrawCard(ImU32 col, float height, std::function<void()> content)
{
    DrawCard(nullptr, col, height, content);
}

void DrawCard(ImU32 col_top, ImU32 col_bot, float height, std::function<void()> content)
{
    DrawCard(nullptr, col_top, col_bot, height, content);
}

void DrawCard(const char* title, ImU32 col, float height, std::function<void()> content)
{
    DrawCard(title, col, col, height, content);
}

void DrawCard(const char* title, ImU32 col_top, ImU32 col_bot, float height, std::function<void()> content)
{
    const bool has_title = (title && title[0]);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float  width = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg_col_top = col_top ? col_top : IM_COL32(28, 28, 32, 255);
    ImU32 bg_col_bot = col_bot ? col_bot : IM_COL32(28, 28, 32, 255);
    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    ImGuiExt::ShadowBoxOuter(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(0, 0, 0, 30), 15.0f, 10.0f);
    ImGuiExt::AddRectFilledMultiColor(pos, ImVec2(pos.x + width, pos.y + height), bg_col_top, bg_col_bot, 10.0f, 0);
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), border_col, 10.0f, 0, 1.0f);

    float header_h = 0.0f;

    if (has_title)
    {
        ImFont* font = ImGui::GetFont();
        float title_font_size = ImGui::GetFontSize() * 1.1f;

        dl->AddText(font, title_font_size, ImVec2(pos.x + 16.0f, pos.y + 12.0f), IM_COL32(255, 255, 255, 255), title);

        float line_y = pos.y + 12.0f + title_font_size + 8.0f;
        dl->AddLine(ImVec2(pos.x + 16.0f, line_y), ImVec2(pos.x + width - 16.0f, line_y), IM_COL32(60, 60, 65, 255), 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + 8.0f, line_y + 8.0f));

        header_h = (line_y + 8.0f) - pos.y;
    }
    else
    {
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 8.0f, pos.y + 8.0f));
        header_h = 8.0f;
    }

    const float inner_pad = 8.0f;
    float child_h = height - header_h - inner_pad;
    if (child_h < 1.0f) child_h = 1.0f;

    const char* safe_title = has_title ? title : "notitle";
    std::string child_id = std::string("##card_") + safe_title;

    ImGui::BeginChild(child_id.c_str(), ImVec2(width - 16.0f, child_h), ImGuiChildFlags_None);
    {
        content();
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, height + 12.0f));
}

void DrawCardSimple(const char* title, float height, std::function<void()> content)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    ImU32 bg_col = IM_COL32(28, 28, 32, 255);
    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    ImGuiExt::ShadowBoxOuter(
        pos,
        ImVec2(pos.x + width, pos.y + height),
        IM_COL32(0, 0, 0, 30),
        15.0f,
        10.0f
    );
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg_col, 10.0f);
    dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), border_col, 10.0f, 0, 1.0f);

    ImFont* font = ImGui::GetFont();
    float title_font_size = ImGui::GetFontSize() * 1.1f;
    dl->AddText(font, title_font_size,
        ImVec2(pos.x + 16.0f, pos.y + 12.0f),
        IM_COL32(255, 255, 255, 255), title);

    float line_y = pos.y + 12.0f + title_font_size + 8.0f;
    dl->AddLine(
        ImVec2(pos.x + 16.0f, line_y),
        ImVec2(pos.x + width - 16.0f, line_y),
        IM_COL32(60, 60, 65, 255), 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 8.0f, line_y + 8.0f));
    ImGui::BeginChild((std::string("##card_") + title).c_str(),
        ImVec2(width - 16.0f, height - (line_y - pos.y) - 16.0f),
        ImGuiChildFlags_None);
    {
        content();
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, height + 12.0f));
}

void DrawBoxShaded(ImVec2 p_pos, ImVec2 p_size, ImU32 col, float rounding, bool shaded, ImGuiChildFlags flags)
{
    const ImU32  bg_col = col ? col : ImGui::GetColorU32(ImGuiCol_ChildBg);

    ImDrawList*  dl = ImGui::GetWindowDrawList();
    if (shaded)
    {
        ImGuiExt::ShadowBoxOuter(
            p_pos,
            ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y),
            IM_COL32(0, 0, 0, 30),
            15.0f,
            rounding
        );
    }

    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    dl->AddRectFilled(p_pos, ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y), bg_col, rounding);
    dl->AddRect(p_pos, ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y), border_col, rounding, 0, 1.0f);
}

void DrawBoxShaded(ImVec2 p_pos, ImVec2 p_size, ImU32 col_top, ImU32 col_bot, float rounding, bool shaded, ImGuiChildFlags flags)
{
    if (!col_top && !col_bot)
    {
        DrawBoxShaded(p_pos, p_size, ImGui::GetColorU32(ImGuiCol_ChildBg), rounding, shaded, flags);
        return;
    }

    if (col_top == col_bot)
    {
        DrawBoxShaded(p_pos, p_size, col_top, rounding, shaded, flags);
        return;
    }

    const ImU32  bg_col_top = col_top ? col_top : ImGui::GetColorU32(ImGuiCol_ChildBg);
    const ImU32  bg_col_bot = col_bot ? col_bot : ImGui::GetColorU32(ImGuiCol_ChildBg);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (shaded)
    {
        ImGuiExt::ShadowBoxOuter(
            p_pos,
            ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y),
            IM_COL32(0, 0, 0, 30),
            15.0f,
            rounding
        );
    }

    ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

    ImGuiExt::AddRectFilledMultiColor(p_pos, ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y), bg_col_top, bg_col_bot, rounding);
    dl->AddRect(p_pos, ImVec2(p_pos.x + p_size.x, p_pos.y + p_size.y), border_col, rounding, 0, 1.0f);
}