#pragma once

#include <imgui.h>
#include <pod_types.hpp>
#include <reflection/enum.hpp>

namespace ImGuiEx
{
    class NonCopyable
    {
    protected:
        NonCopyable()  = default;
        ~NonCopyable() = default;

    private:
        NonCopyable(const NonCopyable&)            = default;
        NonCopyable& operator=(const NonCopyable&) = default;
    };

    class ScopedColor : public NonCopyable
    {
    public:
        template<typename T>
        ScopedColor(ImGuiCol colorId, T color)
        {
            ImGui::PushStyleColor(colorId, ImColor(color).Value);
        }

        ~ScopedColor() { ImGui::PopStyleColor(); }
    };

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

    template<typename T>
    bool Bits(T& mask, const char* const names[], u32 count, const u32 columns = 2)
        requires(std::is_integral_v<T>)
    {
        bool modified = false;
        ImGui::PushID(&mask);

        constexpr u32 BITS = sizeof(T) * 8;
        const T all_mask   = (count >= BITS) ? T(~T(0)) : T((T(1) << count) - 1);

        // Header: hex value + batch buttons
        ImGui::Text("0x%llX", static_cast<u64>(mask));
        ImGui::SameLine();
        if (ImGui::SmallButton("All"))
        {
            mask     = all_mask;
            modified = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("None"))
        {
            mask     = T(0);
            modified = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Invert"))
        {
            mask ^= all_mask;
            modified = true;
        }

        // Checkboxes in a tight grid; green checkmark for set bits
        if (ImGui::BeginTable("bits", static_cast<int>(columns), ImGuiTableFlags_SizingStretchSame))
        {
            ScopedColor mark(ImGuiCol_CheckMark, IM_COL32(120, 230, 130, 255));

            for (u32 i = 0; i < count; ++i)
            {
                ImGui::TableNextColumn();

                const T bit_flag = T(1) << i;
                bool bit_value   = (mask & bit_flag) != 0;

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
}
