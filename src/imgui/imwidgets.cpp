#include <glm/ext/matrix_clip_space.hpp>
#include <imgui/imwidgets.hpp>
#include <ImGuizmo.h>
#include <reflection/enum.hpp>

#include <glm/gtx/norm.inl>

namespace
{
    glm::mat4 build_gizmo_proj(const glm::mat4& proj)
    {
        f32 zfar  = 0.0F;
        f32 znear = 0.0F;

        if (proj[2][2] == 0.0F)
        {
            constexpr f32 kDummyFar = 10000.0F;

            zfar  = kDummyFar;
            znear = proj[3][2];
        }
        else
        {
            znear = proj[3][2] / (proj[2][2] + 1);
            zfar  = proj[3][2] / proj[2][2];
        }

        auto guizmo_proj  = glm::mat4(0.0F);
        guizmo_proj[0][0] = proj[0][0];
        guizmo_proj[1][1] = -proj[1][1];
        guizmo_proj[2][2] = -(zfar + znear) / (zfar - znear);
        guizmo_proj[2][3] = -1.0F;
        guizmo_proj[3][2] = -(2.0F * zfar * znear) / (zfar - znear);

        return guizmo_proj;
    }

    bool world_to_screen(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& world_pos,
                         const ImVec2& viewport_size, ImVec2& out_screen)
    {
        const glm::vec4 clip = proj * view * glm::vec4(world_pos, 1.0F);

        if (clip.w <= 0.0001F)
        {
            return false;
        }

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;

        out_screen.x = (ndc.x * 0.5F + 0.5F) * viewport_size.x;
        out_screen.y = (1.0F - (ndc.y * 0.5F + 0.5F)) * viewport_size.y;

        return true;
    }

    const char* gizmo_op_to_str(const ImGuizmo::OPERATION op)
    {
        switch (op)
        {
        case ImGuizmo::OPERATION::TRANSLATE :
            return "Translate";
        case ImGuizmo::OPERATION::ROTATE :
            return "Rotate";
        case ImGuizmo::OPERATION::SCALE :
            return "Scale";
        default :
            return "Unknown";
        }
    }

    void gizmo_op_button(const char* label, const ImGuizmo::OPERATION value, ImGuizmo::OPERATION& current)
    {
        const bool selected = current == value;

        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }

        if (ImGui::SmallButton(label))
        {
            current = value;
        }

        if (selected)
        {
            ImGui::PopStyleColor();
        }
    }

    ImVec2 clamp_menu_pos(ImVec2 pos, const ImVec2& display_size)
    {
        constexpr float kMenuWidth  = 130.0F;
        constexpr float kMenuHeight = 42.0F;

        pos.x = glm::clamp(pos.x, 8.0F, display_size.x - kMenuWidth);
        pos.y = glm::clamp(pos.y, 8.0F, display_size.y - kMenuHeight);

        return pos;
    }

    bool gizmo_op_impl(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& source,
                       ImGuizmo::OPERATION& operation)
    {
        ImGuiIO& io = ImGui::GetIO();

        const glm::vec3 origin = glm::vec3(source[3]);

        const glm::vec3 camera_pos = glm::vec3(glm::inverse(view)[3]);
        const f32 camera_distance  = glm::length(camera_pos - origin);
        const f32 axis_probe_len   = glm::clamp(camera_distance * 0.12F, 0.25F, 10.0F);

        auto x_axis = glm::vec3(source[0]);
        auto y_axis = glm::vec3(source[1]);
        auto z_axis = glm::vec3(source[2]);

        x_axis = glm::normalize(glm::length2(x_axis) < 0.0001F ? glm::vec3(1.0F, 0.0F, 0.0F) : x_axis);
        y_axis = glm::normalize(glm::length2(y_axis) < 0.0001F ? glm::vec3(0.0F, 1.0F, 0.0F) : y_axis);
        z_axis = glm::normalize(glm::length2(z_axis) < 0.0001F ? glm::vec3(0.0F, 0.0F, 1.0F) : z_axis);

        ImVec2 screen_origin {};
        ImVec2 screen_x {};
        ImVec2 screen_y {};
        ImVec2 screen_z {};

        const bool origin_visible = world_to_screen(view, proj, origin, io.DisplaySize, screen_origin);

        if (!origin_visible)
        {
            return false;
        }

        const bool x_visible = world_to_screen(view, proj, origin + x_axis * axis_probe_len, io.DisplaySize, screen_x);
        const bool y_visible = world_to_screen(view, proj, origin + y_axis * axis_probe_len, io.DisplaySize, screen_y);
        const bool z_visible = world_to_screen(view, proj, origin + z_axis * axis_probe_len, io.DisplaySize, screen_z);

        f32 max_x = screen_origin.x;
        f32 min_y = screen_origin.y;

        if (x_visible)
        {
            max_x = glm::max(max_x, screen_x.x);
            min_y = glm::min(min_y, screen_x.y);
        }

        if (y_visible)
        {
            max_x = glm::max(max_x, screen_y.x);
            min_y = glm::min(min_y, screen_y.y);
        }

        if (z_visible)
        {
            max_x = glm::max(max_x, screen_z.x);
            min_y = glm::min(min_y, screen_z.y);
        }

        ImVec2 menu_pos = clamp_menu_pos(ImVec2(max_x + 12.0F, min_y - 8.0F), io.DisplaySize);

        ImGui::SetNextWindowPos(menu_pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75F);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                                         | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove
                                         | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

        bool hovered = false;
        {
            ImGuiEx::ScopedBegin _("##gizmo_operation_selector", nullptr, flags);
            ImGui::TextUnformatted(gizmo_op_to_str(operation));

            gizmo_op_button("T", ImGuizmo::OPERATION::TRANSLATE, operation);
            ImGui::SameLine();
            gizmo_op_button("R", ImGuizmo::OPERATION::ROTATE, operation);
            ImGui::SameLine();
            gizmo_op_button("S", ImGuizmo::OPERATION::SCALE, operation);
            hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        }

        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        draw_list->AddLine(screen_origin, ImVec2(menu_pos.x, menu_pos.y + 18.0F), IM_COL32(255, 255, 255, 80), 1.0F);

        return hovered;
    }
}

bool ImGuiWidgets::ButtonCheckbox(const char* label, bool& value, u32 active_color, u32 inactive_color)
{
    ImGuiEx::ScopedColor col(ImGuiCol_Text, value ? active_color : inactive_color);
    if (ImGui::Button(label))
    {
        value = !value;
        return true;
    }
    return false;
}

bool ImGuiWidgets::ButtonAccent(const char* label, const ImVec2& size)
{
    ImGuiEx::ScopedColor bc(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.70f));
    ImGuiEx::ScopedColor bca(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 1.00f));
    ImGuiEx::ScopedColor bch(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.90f));

    ImGuiEx::ScopedStyleVar fr(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGuiEx::ScopedStyleVar fp(ImGuiStyleVar_FramePadding, ImVec2(16, 10));

    return ImGui::Button(label, size);
}

bool ImGuiWidgets::Bits(u32& mask, const char* const names[], u32 count, const u32 columns)
{
    bool modified = false;
    ImGui::PushID(&mask);

    constexpr u32 kMaskAll = ~0U;

    // Header: hex value + batch buttons
    ImGui::Text("0x%llX", static_cast<u64>(mask));
    ImGui::SameLine();
    if (ImGui::SmallButton("All"))
    {
        mask     = kMaskAll;
        modified = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("None"))
    {
        mask     = 0;
        modified = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Invert"))
    {
        mask ^= kMaskAll;
        modified = true;
    }

    // Checkboxes in a tight grid; green checkmark for set bits
    if (ImGui::BeginTable("bits", static_cast<int>(columns), ImGuiTableFlags_SizingStretchSame))
    {
        ImGuiEx::ScopedColor mark(ImGuiCol_CheckMark, IM_COL32(120, 230, 130, 255));

        for (u32 i = 0; i < count; ++i)
        {
            ImGui::TableNextColumn();

            const u32 bit_flag = 1 << i;
            bool bit_value     = (mask & bit_flag) != 0;

            if (ImGui::Checkbox(names[i], &bit_value))
            {
                mask     = bit_value ? (mask | bit_flag) : (mask & ~bit_flag);
                modified = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("bit %u  (0x%llX)", i, static_cast<u64>(bit_flag));
            }
        }
        ImGui::EndTable();
    }

    ImGui::PopID();
    return modified;
}

void ImGuiWidgets::Gizmo(const glm::mat4& view, const glm::mat4& proj, glm::mat4& source,
                         ImGuizmo::OPERATION& operation)
{
    ImGuiIO& io      = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // link from the gizmo origin to the operation selector.

    const auto gizmo_proj = build_gizmo_proj(proj);
    ImGuizmo::Enable(!gizmo_op_impl(view, gizmo_proj, source, operation));

    ImGuizmo::SetDrawlist(draw);
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(&view[0][0], &gizmo_proj[0][0], operation, ImGuizmo::MODE::WORLD, &source[0][0]);

    ImGuizmo::Enable(true);
}

bool ImGuiWidgets::GizmoOp(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& source,
                           ImGuizmo::OPERATION& operation)
{
    return gizmo_op_impl(view, build_gizmo_proj(proj), source, operation);
}
