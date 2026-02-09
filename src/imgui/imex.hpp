#pragma once

#include <types.hpp>

#include <imgui.h>

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

        ~ScopedColor()
        {
            ImGui::PopStyleColor();
        }
    };

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
