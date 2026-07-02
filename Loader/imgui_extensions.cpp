#define IMGUI_DEFINE_MATH_OPERATORS 

#include "imgui_extensions.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace ImGuiExt {
    static const float shadow_base_dist = 4.0f;
    static const float shadow_hover_dist = 10.0f;
    static const int   shadow_base_alpha = 40;
    static const int   shadow_hover_alpha = 80;
    static const float shadow_offset_y = 2.0f;

    bool IconizedButton(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        ImU32 accent_color,
        ImU32 highlight_color,
        bool selected,
        ImGuiButtonFlags flags,
        float spacing,
        const ImVec2& size_arg)
    {
        return IconizedButtonEx(
            tex_id, label, image_size,
            accent_color, highlight_color, selected,
            ImVec2(0, 0), ImVec2(1, 1),
            ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1),
            flags, spacing, size_arg
        );
    }

    bool IconizedButtonEx(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        ImU32 accent_color,
        ImU32 highlight_color,
        bool selected,
        const ImVec2& uv0,
        const ImVec2& uv1,
        const ImVec4& bg_col,
        const ImVec4& tint_col,
        ImGuiButtonFlags flags,
        float spacing,
        const ImVec2& size_arg)
    {
        return IconizedButtonEx(
            tex_id, label, image_size, selected, 
            uv0, uv1, bg_col, tint_col, flags, spacing, 
            size_arg, accent_color, highlight_color
        );
    }

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
        ImU32 highlight_color)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);
        const bool has_text = label_size.x > 0.0f;

        const float content_w = image_size.x + (has_text ? (spacing + label_size.x) : 0.0f);
        const float content_h = ImMax(image_size.y, label_size.y);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;

        ImVec2 size = ImGui::CalcItemSize(size_arg, content_w + style.FramePadding.x * 2.0f, content_h + style.FramePadding.y * 2.0f);
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        ImU32 frame_col;
        if (held && hovered) frame_col = (accent_color != 0) ? accent_color : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        else if (hovered) frame_col = (highlight_color != 0) ? highlight_color : ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else frame_col = ImGui::GetColorU32(ImGuiCol_Button);

        ImGui::RenderNavCursor(bb, id);
        ImGui::RenderFrame(bb.Min, bb.Max, frame_col, true, style.FrameRounding);

        const ImVec2 align = style.ButtonTextAlign;
        const float avail_x = bb.GetWidth() - style.FramePadding.x * 2.0f;
        const float avail_y = bb.GetHeight() - style.FramePadding.y * 2.0f;
        const float offset_x = ImMax(0.0f, (avail_x - content_w) * align.x);

        const float img_x = bb.Min.x + style.FramePadding.x + offset_x;
        const float img_y = bb.Min.y + style.FramePadding.y + ImMax(0.0f, (avail_y - image_size.y) * align.y);
        const ImRect img_bb(ImVec2(img_x, img_y), ImVec2(img_x + image_size.x, img_y + image_size.y));

        if (bg_col.w > 0.0f)
            window->DrawList->AddRectFilled(img_bb.Min, img_bb.Max, ImGui::GetColorU32(bg_col));

        float image_rounding = ImMax(style.FrameRounding - ImMax(style.FramePadding.x, style.FramePadding.y), style.ImageRounding);
        if (image_rounding > 0.0f)
            window->DrawList->AddImageRounded(tex_id, img_bb.Min, img_bb.Max, uv0, uv1, ImGui::GetColorU32(tint_col), image_rounding);
        else
            window->DrawList->AddImage(tex_id, img_bb.Min, img_bb.Max, uv0, uv1, ImGui::GetColorU32(tint_col));

        if (has_text)
        {
            if (g.LogEnabled)
                ImGui::LogSetNextTextDecoration("[", "]");

            const float text_x = img_bb.Max.x + spacing;
            const float text_y = bb.Min.y + style.FramePadding.y + ImMax(0.0f, (avail_y - label_size.y) * align.y);
            const ImRect text_bb(ImVec2(text_x, text_y), ImVec2(text_x + label_size.x, text_y + label_size.y));

            ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, label, label_end, &label_size, align, &bb);
        }

        if (selected)
        {
            window->DrawList->PushClipRectFullScreen();

            const float indicator_width = 4.0f;
            const float indicator_height = bb.GetHeight() * 0.6f;
            const float offset = 3.0f;

            float x = bb.Min.x - offset;
            float y = bb.Min.y + (bb.GetHeight() - indicator_height) * 0.5f;

            window->DrawList->AddRectFilled(
                ImVec2(x - indicator_width, y),
                ImVec2(x, y + indicator_height),
                accent_color,
                2.0f
            );

            if ((selected || hovered || held) && accent_color != 0)
            {
                window->DrawList->AddRect(bb.Min, bb.Max, accent_color, style.FrameRounding, 0, 1.5f);
            }

            window->DrawList->PopClipRect();
        }

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    void AddRectFilledMultiColor(const ImVec2& p_min, const ImVec2& p_max, ImU32 col_up, ImU32 col_bot, float rounding, ImDrawFlags flags)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (p_max.y <= p_min.y)
            return;

        if (col_up == col_bot)
        {
            draw_list->AddRectFilled(p_min, p_max, col_up, rounding, flags);
            return;
        }

        if (rounding <= 0.0f || (flags & ImDrawFlags_RoundCornersAll) == 0)
        {
            draw_list->AddRectFilledMultiColor(p_min, p_max, col_up, col_up, col_bot, col_bot);
            return;
        }

        const int vtx_before = draw_list->VtxBuffer.Size;

        draw_list->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);

        const int vtx_after = draw_list->VtxBuffer.Size;

        ImVec4 color_up = ImGui::ColorConvertU32ToFloat4(col_up);
        ImVec4 color_bot = ImGui::ColorConvertU32ToFloat4(col_bot);

        float inv_h = 1.0f / (p_max.y - p_min.y);

        for (int i = vtx_before; i < vtx_after; i++)
        {
            ImDrawVert* vert = draw_list->VtxBuffer.Data + i;

            float t = (vert->pos.y - p_min.y) * inv_h;
            t = ImClamp(t, 0.0f, 1.0f);

            float r = color_up.x + (color_bot.x - color_up.x) * t;
            float g = color_up.y + (color_bot.y - color_up.y) * t;
            float b = color_up.z + (color_bot.z - color_up.z) * t;
            float a = color_up.w + (color_bot.w - color_up.w) * t;

            ImVec4 orig_col = ImGui::ColorConvertU32ToFloat4(vert->col);
            r *= orig_col.x;
            g *= orig_col.y;
            b *= orig_col.z;
            a *= orig_col.w;

            vert->col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
        }
    }

    void AddRectFilledMultiColorHorizontal(const ImVec2& p_min, const ImVec2& p_max, ImU32 col_left, ImU32 col_right, float rounding, ImDrawFlags flags)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (p_max.x <= p_min.x)
            return;

        if (col_left == col_right)
        {
            draw_list->AddRectFilled(p_min, p_max, col_left, rounding, flags);
            return;
        }

        if (rounding <= 0.0f || (flags & ImDrawFlags_RoundCornersAll) == 0)
        {
            draw_list->AddRectFilledMultiColor(p_min, p_max, col_left, col_right, col_right, col_left);
            return;
        }

        const int vtx_before = draw_list->VtxBuffer.Size;
        draw_list->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
        const int vtx_after = draw_list->VtxBuffer.Size;

        ImVec4 color_left = ImGui::ColorConvertU32ToFloat4(col_left);
        ImVec4 color_right = ImGui::ColorConvertU32ToFloat4(col_right);

        float inv_w = 1.0f / (p_max.x - p_min.x);

        for (int i = vtx_before; i < vtx_after; i++)
        {
            ImDrawVert* vert = draw_list->VtxBuffer.Data + i;

            float t = (vert->pos.x - p_min.x) * inv_w;
            t = ImClamp(t, 0.0f, 1.0f);

            float r = color_left.x + (color_right.x - color_left.x) * t;
            float g = color_left.y + (color_right.y - color_left.y) * t;
            float b = color_left.z + (color_right.z - color_left.z) * t;
            float a = color_left.w + (color_right.w - color_left.w) * t;

            ImVec4 orig_col = ImGui::ColorConvertU32ToFloat4(vert->col);
            r *= orig_col.x;
            g *= orig_col.y;
            b *= orig_col.z;
            a *= orig_col.w;

            vert->col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
        }
    }

    static float SdfRoundedBox(const ImVec2& p, const ImVec2& box_min, const ImVec2& box_max, float rounding)
    {
        ImVec2 center = ImVec2((box_min.x + box_max.x) * 0.5f, (box_min.y + box_max.y) * 0.5f);
        ImVec2 half = ImVec2((box_max.x - box_min.x) * 0.5f, (box_max.y - box_min.y) * 0.5f);

        ImVec2 local = ImVec2(ImFabs(p.x - center.x), ImFabs(p.y - center.y));

        ImVec2 d = ImVec2(local.x - (half.x - rounding), local.y - (half.y - rounding));

        float outside = ImSqrt(ImMax(d.x, 0.0f) * ImMax(d.x, 0.0f) + ImMax(d.y, 0.0f) * ImMax(d.y, 0.0f));
        float inside = ImMin(ImMax(d.x, d.y), 0.0f);

        return outside + inside - rounding;
    }

    void ShadowBoxInner(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags)
    {
        if (dist <= 0.0f) return;
        if ((col & IM_COL32_A_MASK) == 0) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        const int base_r = (col >> IM_COL32_R_SHIFT) & 0xFF;
        const int base_g = (col >> IM_COL32_G_SHIFT) & 0xFF;
        const int base_b = (col >> IM_COL32_B_SHIFT) & 0xFF;
        const int base_a = (col >> IM_COL32_A_SHIFT) & 0xFF;

        const int vtx_before = dl->VtxBuffer.Size;
        dl->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
        const int vtx_after = dl->VtxBuffer.Size;

        for (int i = vtx_before; i < vtx_after; i++)
        {
            ImDrawVert* vert = dl->VtxBuffer.Data + i;

            float d = -SdfRoundedBox(vert->pos, p_min, p_max, rounding);
            d = ImMax(d, 0.0f);

            float t = ImClamp(d / dist, 0.0f, 1.0f);

            float alpha_factor = 1.0f - t;
            alpha_factor = alpha_factor * alpha_factor;

            int a = (int)(base_a * alpha_factor);

            ImVec4 orig = ImGui::ColorConvertU32ToFloat4(vert->col);
            a = (int)(a * orig.w);

            vert->col = IM_COL32(base_r, base_g, base_b, a);
        }
    }

    void ShadowBoxOuter(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags)
    {
        if (dist <= 0.0f) return;
        if ((col & IM_COL32_A_MASK) == 0) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImVec2 outer_min = ImVec2(p_min.x - dist, p_min.y - dist);
        ImVec2 outer_max = ImVec2(p_max.x + dist, p_max.y + dist);

        float outer_rounding = rounding + dist;

        const int base_r = (col >> IM_COL32_R_SHIFT) & 0xFF;
        const int base_g = (col >> IM_COL32_G_SHIFT) & 0xFF;
        const int base_b = (col >> IM_COL32_B_SHIFT) & 0xFF;
        const int base_a = (col >> IM_COL32_A_SHIFT) & 0xFF;

        const int vtx_before = dl->VtxBuffer.Size;
        dl->AddRectFilled(outer_min, outer_max, IM_COL32_WHITE, outer_rounding, flags);
        const int vtx_after = dl->VtxBuffer.Size;

        for (int i = vtx_before; i < vtx_after; i++)
        {
            ImDrawVert* vert = dl->VtxBuffer.Data + i;
            float d = SdfRoundedBox(vert->pos, p_min, p_max, rounding);
            float t = ImClamp(d / dist, 0.0f, 1.0f);
            float alpha_factor = 1.0f - t;
            alpha_factor = alpha_factor * alpha_factor;

            int a = (int)(base_a * alpha_factor);

            vert->col = IM_COL32(base_r, base_g, base_b, a);
        }
    }
}