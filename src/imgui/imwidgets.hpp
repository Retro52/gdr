#pragma once

#include <glm/mat4x4.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui/imex.hpp>
#include <pod_types.hpp>
#include <reflection/enum.hpp>

#include <utility>

namespace ImGuiWidgets
{
    // Common text styles
    template<typename... Args>
    void TextError(const char* fmt, Args&&... args)
    {
        ImGuiEx::ScopedColor col(ImGuiCol_Text, IM_COL32(204, 51, 0, 255));
        ImGui::Text(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void TextWarning(const char* fmt, Args&&... args)
    {
        ImGuiEx::ScopedColor col(ImGuiCol_Text, IM_COL32(255, 204, 0, 255));
        ImGui::Text(fmt, std::forward<Args>(args)...);
    }

    bool ButtonCheckbox(const char* label, bool& value, u32 active_color = IM_COL32(255, 255, 255, 255),
                        u32 inactive_color = IM_COL32(77, 77, 77, 255));

    // Accent button (blue-ish) with rounded corners cause we suck up to modern trends
    bool ButtonAccent(const char* label, const ImVec2& size = {0, 0});

    bool Bits(u32& mask, const char* const names[], u32 count, u32 columns = 2);

    void Gizmo(const glm::mat4& view, const glm::mat4& proj, glm::mat4& source, ImGuizmo::OPERATION& operation);

    bool GizmoOp(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& source, ImGuizmo::OPERATION& operation);

    template<typename T>
    bool Enum(const char* label, T& evalue)
    {
        bool return_value        = false;
        const u32 current_casted = static_cast<u32>(evalue);
        if (ImGui::BeginCombo(label, reflection::string_from_enum<T>(evalue)))
        {
            const auto* candidates = reflection::get_enum_values<T>();
            for (u32 i = 0; i < reflection::get_enum_values_count<T>(); ++i)
            {
                const reflection::enum_value& candidate = candidates[i];
                const bool selected                     = current_casted == candidate.value;

                if (ImGui::Selectable(candidate.name, selected))
                {
                    return_value = true;
                    evalue       = static_cast<T>(candidate.value);
                }
            }

            ImGui::EndCombo();
        }

        return return_value;
    }
}
