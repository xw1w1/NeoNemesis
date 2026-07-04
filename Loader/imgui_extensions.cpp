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

    bool Button(
        const char* label,
        ImU32 accent_color,
        ImU32 highlight_color,
        const ImVec2& size_arg,
        ImGuiButtonFlags flags)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const char* label_end = ImGui::FindRenderedTextEnd(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, label_end, false);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;

        const ImVec2 content_size = label_size;
        const ImVec2 frame_size = ImVec2(
            content_size.x + style.FramePadding.x * 2.0f,
            content_size.y + style.FramePadding.y * 2.0f
        );

        const ImVec2 size = ImGui::CalcItemSize(size_arg, frame_size.x, frame_size.y);
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        ImU32 frame_col;
        if (held && hovered) frame_col = (accent_color != 0) ? accent_color : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        else if (hovered) frame_col = (highlight_color != 0) ? highlight_color : ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else frame_col = ImGui::GetColorU32(ImGuiCol_Button);

        ImGui::RenderNavCursor(bb, id);
        ShadowBoxOuter(
            pos, pos + size,
            IM_COL32(0, 0, 0, 30),
            12.0f,
            style.FrameRounding
        );
        ImGui::RenderFrame(bb.Min, bb.Max, frame_col, true, style.FrameRounding);

        const ImVec2 align = style.ButtonTextAlign;
        const float avail_x = bb.GetWidth() - style.FramePadding.x * 2.0f;
        const float avail_y = bb.GetHeight() - style.FramePadding.y * 2.0f;

        const float text_x = bb.Min.x + style.FramePadding.x + ImMax(0.0f, (avail_x - label_size.x) * align.x);
        const float text_y = bb.Min.y + style.FramePadding.y + ImMax(0.0f, (avail_y - label_size.y) * align.y);

        const ImRect text_bb(ImVec2(text_x, text_y), ImVec2(text_x + label_size.x, text_y + label_size.y));

        if (g.LogEnabled)
            ImGui::LogSetNextTextDecoration("[", "]");

        ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, label, label_end, &label_size, ImVec2(0.0f, 0.0f), &bb);

        if (!held && accent_color != 0)
        {
            window->DrawList->AddRect(bb.Min, bb.Max, accent_color, style.FrameRounding, 0, 1.5f);
        }

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool Toggle(
        const char* idx,
        bool* value,
        ImU32 accent_color,
        ImU32 highlight_color,
        const ImVec2& size_arg,
        ImGuiButtonFlags flags)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(idx);

        const float default_height = ImGui::GetFrameHeight();
        const float default_width = default_height * 2.0f;

        ImVec2 pos = window->DC.CursorPos;
        const ImVec2 size = ImGui::CalcItemSize(size_arg, default_width, default_height);
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered = false;
        bool held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        if (pressed && value)
            *value = !(*value);

        const bool on = value ? *value : false;

        ImGuiStorage* storage = window->DC.StateStorage;
        const ImGuiID anim_id = id + 1;

        float anim = storage->GetFloat(anim_id, on ? 1.0f : 0.0f);
        const float target = on ? 1.0f : 0.0f;
        const float anim_speed = 12.0f;

        if (anim != target)
        {
            float delta = g.IO.DeltaTime * anim_speed;
            if (anim < target)
                anim = ImMin(anim + delta, target);
            else
                anim = ImMax(anim - delta, target);
            storage->SetFloat(anim_id, anim);
        }

        ImU32 bg_col;
        if (on)
        {
            bg_col = (highlight_color != 0) ? highlight_color : ImGui::GetColorU32(ImGuiCol_ButtonActive);
        }
        else
        {
            bg_col = ImGui::GetColorU32(ImGuiCol_Button);
        }

        if (hovered)
        {
            ImVec4 bg_vec = ImGui::ColorConvertU32ToFloat4(bg_col);
            bg_vec.x = ImMin(bg_vec.x + 0.05f, 1.0f);
            bg_vec.y = ImMin(bg_vec.y + 0.05f, 1.0f);
            bg_vec.z = ImMin(bg_vec.z + 0.05f, 1.0f);
            bg_col = ImGui::ColorConvertFloat4ToU32(bg_vec);
        }

        ImGui::RenderNavCursor(bb, id);

        const float rounding = size.y * 0.5f;

        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col, rounding);

        const ImU32 border_col = (accent_color != 0) ? accent_color : ImGui::GetColorU32(ImGuiCol_Border);
        window->DrawList->AddRect(bb.Min, bb.Max, border_col, rounding, 0, 1.5f);

        const float knob_padding = 3.0f;
        const float knob_radius = (size.y - knob_padding * 2.0f) * 0.5f;

        const float knob_x_left = bb.Min.x + knob_padding + knob_radius;
        const float knob_x_right = bb.Max.x - knob_padding - knob_radius;

        const float knob_x = ImLerp(knob_x_right, knob_x_left, anim);
        const float knob_y = bb.Min.y + size.y * 0.5f;

        const ImU32 knob_col = (accent_color != 0) ? accent_color : ImGui::GetColorU32(ImGuiCol_Text);

        window->DrawList->AddCircleFilled(ImVec2(knob_x, knob_y), knob_radius, knob_col);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }

    bool IconButton(
        ImTextureID tex_id,
        const char* label,
        const ImVec2& image_size,
        ImU32 accent_color,
        ImU32 highlight_color,
        bool selected,
        ImGuiButtonFlags flags,
        const ImVec2& size_arg)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        ImVec2 pos = window->DC.CursorPos;

        const ImVec2 size = ImGui::CalcItemSize(
            size_arg,
            image_size.x + style.FramePadding.x * 2.0f,
            image_size.y + style.FramePadding.y * 2.0f
        );
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered = false, held = false;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        ImU32 frame_col;
        if (held && hovered)
            frame_col = (accent_color != 0) ? accent_color : ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        else if (hovered)
            frame_col = (highlight_color != 0) ? highlight_color : ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else
            frame_col = ImGui::GetColorU32(ImGuiCol_Button);

        ImGui::RenderNavCursor(bb, id);

        ShadowBoxOuter(
            pos, pos + size,
            IM_COL32(0, 0, 0, 30),
            12.0f,
            style.FrameRounding
        );

        ImGui::RenderFrame(bb.Min, bb.Max, frame_col, true, style.FrameRounding);

        const float img_x = bb.Min.x + (size.x - image_size.x) * 0.5f;
        const float img_y = bb.Min.y + (size.y - image_size.y) * 0.5f;
        const ImVec2 img_min(img_x, img_y);
        const ImVec2 img_max(img_x + image_size.x, img_y + image_size.y);

        float image_rounding = ImMax(
            style.FrameRounding - ImMax(style.FramePadding.x, style.FramePadding.y),
            style.ImageRounding
        );

        if (image_rounding > 0.0f)
        {
            window->DrawList->AddImageRounded(
                tex_id, img_min, img_max,
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32_WHITE, image_rounding
            );
        }
        else
        {
            window->DrawList->AddImage(
                tex_id, img_min, img_max,
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32_WHITE
            );
        }

        if (selected && accent_color != 0)
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

            if ((selected || hovered || held))
            {
                window->DrawList->AddRect(
                    bb.Min, bb.Max,
                    accent_color, style.FrameRounding, 0, 1.5f
                );
            }

            window->DrawList->PopClipRect();
        }

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
        return pressed;
    }


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
        ShadowBoxOuter(
            pos, pos + size,
            IM_COL32(0, 0, 0, 30),
            12.0f,
            style.FrameRounding
        );
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
        ImVec2 center((box_min.x + box_max.x) * 0.5f, (box_min.y + box_max.y) * 0.5f);
        ImVec2 half((box_max.x - box_min.x) * 0.5f, (box_max.y - box_min.y) * 0.5f);
        ImVec2 local(ImFabs(p.x - center.x), ImFabs(p.y - center.y));
        ImVec2 d(local.x - (half.x - rounding), local.y - (half.y - rounding));

        float outside = ImSqrt(ImMax(d.x, 0.0f) * ImMax(d.x, 0.0f) +
            ImMax(d.y, 0.0f) * ImMax(d.y, 0.0f));
        float inside = ImMin(ImMax(d.x, d.y), 0.0f);
        return outside + inside - rounding;
    }

    static float ShadowFalloff(float t)
    {
        float x = 1.0f - t;
        return x * x * x;
    }

    void ShadowBoxInner(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags)
    {
        if (dist <= 0.0f) return;
        if ((col & IM_COL32_A_MASK) == 0) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 col_opaque = col;
        ImU32 col_transparent = col & ~IM_COL32_A_MASK;

        float box_w = p_max.x - p_min.x;
        float box_h = p_max.y - p_min.y;
        float r = ImMin(rounding, ImMin(box_w, box_h) * 0.5f);
        r = ImMax(r, 0.0f);

        float d = ImMin(dist, ImMin(box_w, box_h) * 0.5f);

        const int corner_segments = 16;

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x + r, p_min.y),
            ImVec2(p_max.x - r, p_min.y + d),
            col_opaque, col_opaque, col_transparent, col_transparent
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x + r, p_max.y - d),
            ImVec2(p_max.x - r, p_max.y),
            col_transparent, col_transparent, col_opaque, col_opaque
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x, p_min.y + r),
            ImVec2(p_min.x + d, p_max.y - r),
            col_opaque, col_transparent, col_transparent, col_opaque
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_max.x - d, p_min.y + r),
            ImVec2(p_max.x, p_max.y - r),
            col_transparent, col_opaque, col_opaque, col_transparent
        );

        auto DrawInnerCorner = [&](ImVec2 corner_center, float angle_start, float angle_end)
            {
                float outer_r = r;
                float inner_r = ImMax(0.0f, r - d);

                for (int i = 0; i < corner_segments; i++)
                {
                    float t0 = (float)i / corner_segments;
                    float t1 = (float)(i + 1) / corner_segments;
                    float a0 = angle_start + (angle_end - angle_start) * t0;
                    float a1 = angle_start + (angle_end - angle_start) * t1;

                    ImVec2 outer0(corner_center.x + cosf(a0) * outer_r, corner_center.y + sinf(a0) * outer_r);
                    ImVec2 outer1(corner_center.x + cosf(a1) * outer_r, corner_center.y + sinf(a1) * outer_r);

                    ImVec2 inner0(corner_center.x + cosf(a0) * inner_r, corner_center.y + sinf(a0) * inner_r);
                    ImVec2 inner1(corner_center.x + cosf(a1) * inner_r, corner_center.y + sinf(a1) * inner_r);

                    dl->PrimReserve(6, 4);
                    ImVec2 uv = dl->_Data->TexUvWhitePixel;

                    dl->PrimWriteVtx(outer0, uv, col_opaque);
                    dl->PrimWriteVtx(outer1, uv, col_opaque);
                    dl->PrimWriteVtx(inner1, uv, col_transparent);
                    dl->PrimWriteVtx(inner0, uv, col_transparent);

                    ImDrawIdx idx = (ImDrawIdx)(dl->_VtxCurrentIdx - 4);
                    dl->PrimWriteIdx(idx);
                    dl->PrimWriteIdx(idx + 1);
                    dl->PrimWriteIdx(idx + 2);
                    dl->PrimWriteIdx(idx);
                    dl->PrimWriteIdx(idx + 2);
                    dl->PrimWriteIdx(idx + 3);
                }
            };

        DrawInnerCorner(ImVec2(p_min.x + r, p_min.y + r), IM_PI, IM_PI * 1.5f);
        DrawInnerCorner(ImVec2(p_max.x - r, p_min.y + r), IM_PI * 1.5f, IM_PI * 2.0f);
        DrawInnerCorner(ImVec2(p_max.x - r, p_max.y - r), 0.0f, IM_PI * 0.5f);
        DrawInnerCorner(ImVec2(p_min.x + r, p_max.y - r), IM_PI * 0.5f, IM_PI);
    }

    void ShadowBoxOuter(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float dist, float rounding, ImDrawFlags flags)
    {
        if (dist <= 0.0f) return;
        if ((col & IM_COL32_A_MASK) == 0) return;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 col_opaque = col;
        ImU32 col_transparent = col & ~IM_COL32_A_MASK;

        float box_w = p_max.x - p_min.x;
        float box_h = p_max.y - p_min.y;
        float r = ImMin(rounding, ImMin(box_w, box_h) * 0.5f);
        r = ImMax(r, 0.0f);

        const int corner_segments = 16;

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x + r, p_min.y - dist),
            ImVec2(p_max.x - r, p_min.y),
            col_transparent, col_transparent, col_opaque, col_opaque
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x + r, p_max.y),
            ImVec2(p_max.x - r, p_max.y + dist),
            col_opaque, col_opaque, col_transparent, col_transparent
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_min.x - dist, p_min.y + r),
            ImVec2(p_min.x, p_max.y - r),
            col_transparent, col_opaque, col_opaque, col_transparent
        );

        dl->AddRectFilledMultiColor(
            ImVec2(p_max.x, p_min.y + r),
            ImVec2(p_max.x + dist, p_max.y - r),
            col_opaque, col_transparent, col_transparent, col_opaque
        );

        auto DrawCorner = [&](ImVec2 corner_center, float angle_start, float angle_end)
            {

                float outer_r = r + dist;

                for (int i = 0; i < corner_segments; i++)
                {
                    float t0 = (float)i / corner_segments;
                    float t1 = (float)(i + 1) / corner_segments;
                    float a0 = angle_start + (angle_end - angle_start) * t0;
                    float a1 = angle_start + (angle_end - angle_start) * t1;

                    ImVec2 inner0(corner_center.x + cosf(a0) * r, corner_center.y + sinf(a0) * r);
                    ImVec2 inner1(corner_center.x + cosf(a1) * r, corner_center.y + sinf(a1) * r);

                    ImVec2 outer0(corner_center.x + cosf(a0) * outer_r, corner_center.y + sinf(a0) * outer_r);
                    ImVec2 outer1(corner_center.x + cosf(a1) * outer_r, corner_center.y + sinf(a1) * outer_r);

                    dl->PrimReserve(6, 4);

                    ImVec2 uv = dl->_Data->TexUvWhitePixel;

                    dl->PrimWriteVtx(inner0, uv, col_opaque);
                    dl->PrimWriteVtx(inner1, uv, col_opaque);
                    dl->PrimWriteVtx(outer1, uv, col_transparent);
                    dl->PrimWriteVtx(outer0, uv, col_transparent);

                    ImDrawIdx idx = (ImDrawIdx)(dl->_VtxCurrentIdx - 4);
                    dl->PrimWriteIdx(idx);
                    dl->PrimWriteIdx(idx + 1);
                    dl->PrimWriteIdx(idx + 2);
                    dl->PrimWriteIdx(idx);
                    dl->PrimWriteIdx(idx + 2);
                    dl->PrimWriteIdx(idx + 3);
                }
            };

        DrawCorner(ImVec2(p_min.x + r, p_min.y + r), IM_PI, IM_PI * 1.5f);

        DrawCorner(ImVec2(p_max.x - r, p_min.y + r), IM_PI * 1.5f, IM_PI * 2.0f);

        DrawCorner(ImVec2(p_max.x - r, p_max.y - r), 0.0f, IM_PI * 0.5f);

        DrawCorner(ImVec2(p_min.x + r, p_max.y - r), IM_PI * 0.5f, IM_PI);
    }
}