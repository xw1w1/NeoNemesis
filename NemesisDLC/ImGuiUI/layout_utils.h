#pragma once

#include "imgui.h"
#include <functional>

void DrawCard(ImU32 col, float height, std::function<void()> content);
void DrawCard(ImU32 col_top, ImU32 col_bot, float height, std::function<void()> content);
void DrawCard(const char* title, ImU32 col, float height, std::function<void()> content);
void DrawCard(const char* title, ImU32 col_top, ImU32 col_bot, float height, std::function<void()> content);

void SettingsSectionHeader(const char* title);
void SettingsRow(const char* label, const char* tooltip = nullptr);
void SettingsSliderInt(const char* label, int* value, int min, int max, const char* tooltip = nullptr);
void SettingsSliderFloat(const char* label, float* value, float min, float max, const char* format = "%.2f", const char* tooltip = nullptr);
void SettingsColorEditU32(const char* label, ImU32* col, const char* tooltip = nullptr);
void SettingsColorEditVec4(const char* label, ImVec4* col, const char* tooltip = nullptr);
void LabelValueRowCopyable(const char* label, const char* value, bool copyable = false, const char* copy_value = nullptr);
void DrawCardSimple(const char* title, float height, std::function<void()> content);
void DrawBoxShaded(ImVec2 p_pos, ImVec2 p_size, ImU32 col, float rounding, bool shaded = false, ImGuiChildFlags flags = 0);
void DrawBoxShaded(ImVec2 p_pos, ImVec2 p_size, ImU32 col_top, ImU32 col_bot, float rounding, bool shaded = false, ImGuiChildFlags flags = 0);