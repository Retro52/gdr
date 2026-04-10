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
    bool Bits(T& mask, const char* const names[], u32 count)
        requires(std::is_integral_v<T>)
    {
        bool modified = false;

        for (size_t i = 0; i < count; ++i)
        {
            const u32 bit_flag = 1u << i;
            bool bit_value     = (mask & bit_flag) != 0;

            if (ImGui::Checkbox(names[i], &bit_value))
            {
                mask     = bit_value ? mask | bit_flag : mask & ~bit_flag;
                modified = true;
            }
        }

        return modified;
    }
}
