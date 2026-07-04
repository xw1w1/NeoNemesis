#pragma once

#include "imgui.h"

namespace ImGuiExt {
    bool Button(
        const char* label,
        ImU32 accent_color,
        ImU32 highlight_color,
        const ImVec2& size_arg = ImVec2(0.0f, 0.0f),
        ImGuiButtonFlags flags = ImGuiButtonFlags_None
    );

    bool Toggle(
        const char* idx,
        bool* value,
        ImU32 accent_color,
        ImU32 highlight_color,
        const ImVec2& size_arg = ImVec2(0.0f, 0.0f),
        ImGuiButtonFlags flags = ImGuiButtonFlags_None
    );

    bool IconizedButton(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        ImU32 accent_color,
        ImU32 highlight_color,
        bool selected = false,
        ImGuiButtonFlags flags = 0,
        float spacing = -1.0f,
        const ImVec2& size_arg = ImVec2(0, 0)
    );

    bool IconizedButtonEx(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        ImU32 accent_color,
        ImU32 highlight_color,
        bool selected = false,
        const ImVec2& uv0 = ImVec2(0, 0),
        const ImVec2& uv1 = ImVec2(1, 1),
        const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
        const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
        ImGuiButtonFlags flags = 0,
        float spacing = -1.0f,
        const ImVec2& size_arg = ImVec2(0, 0)
    );

    bool IconizedButtonEx(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        bool selected,
        const ImVec2& uv0,
        const ImVec2& uv1,
        const ImVec4& bg_col,
        const ImVec4& tint_col,
        ImGuiButtonFlags flags,
        float spacing,
        const ImVec2& size_arg,
        ImU32 accent_color,
        ImU32 highlight_color
    );

    void AddRectFilledMultiColor(
        const ImVec2& p_min,
        const ImVec2& p_max,
        ImU32 col_up,
        ImU32 col_bot,
        float rounding,
        ImDrawFlags flags = 0
    );

    void AddRectFilledMultiColorHorizontal(
        const ImVec2& p_min,
        const ImVec2& p_max,
        ImU32 col_left,
        ImU32 col_right,
        float rounding,
        ImDrawFlags flags = 0
    );

    void ShadowBoxInner(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags = 0);

    void ShadowBoxOuter(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags = 0);
}