#pragma once

#include "imgui.h"
#include <functional>

void SettingsSectionHeader(const char* title);
void SettingsRow(const char* label, const char* tooltip = nullptr);
void SettingsSliderInt(const char* label, int* value, int min, int max, const char* tooltip = nullptr);
void SettingsSliderFloat(const char* label, float* value, float min, float max, const char* format = "%.2f", const char* tooltip = nullptr);
void SettingsColorEditU32(const char* label, ImU32* col, const char* tooltip = nullptr);
void SettingsColorEditVec4(const char* label, ImVec4* col, const char* tooltip = nullptr);
void LabelValueRowCopyable(const char* label, const char* value, bool copyable = false, const char* copy_value = nullptr);
void DrawCardSimple(const char* title, float height, std::function<void()> content);