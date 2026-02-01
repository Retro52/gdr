#pragma once

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
}
