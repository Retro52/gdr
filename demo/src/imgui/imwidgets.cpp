#include <glm/ext/matrix_clip_space.hpp>
#include <imgui/imwidgets.hpp>
#include <reflection/enum.hpp>

#include <glm/gtx/norm.inl>
#include <ImGuizmo.h>
#include <string_view>
#include <unordered_map>

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
                         const ImGuizmo::OPERATION operation)
{
    ImGuiIO& io      = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    const auto gizmo_proj = build_gizmo_proj(proj);

    ImGuizmo::SetDrawlist(draw);
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(&view[0][0], &gizmo_proj[0][0], operation, ImGuizmo::MODE::WORLD, &source[0][0]);
}

ImGuiWidgets::CommonImageControls ImGuiWidgets::ImageControls(const char* id, f32 mips_count, f32 layers_count)
{
    static std::unordered_map<std::string_view, CommonImageControls> s_storage;
    auto& data = s_storage[id];

    if (mips_count > 0.0F)
    {
        ImGui::SliderFloat(core::stack_string::make_formatted("Mip##%s", id).c_str(), &data.mip, 0.0F, mips_count);
    }
    else
    {
        ImGui::DragFloat(core::stack_string::make_formatted("Mip##%s", id).c_str(), &data.mip, 1.0F);
    }

    if (layers_count > 0.0F)
    {
        ImGui::SliderFloat(
            core::stack_string::make_formatted("Layer##%s", id).c_str(), &data.layer, 0.0F, layers_count);
    }

    return data;
}
