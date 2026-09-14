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

    class ScopedBegin : public NonCopyable
    {
    public:
        explicit ScopedBegin(const char* name, bool* open = nullptr, const ImGuiWindowFlags flags = 0)
        {
            ImGui::Begin(name, open, flags);
        }

        ~ScopedBegin() { ImGui::End(); }
    };

    class ScopedColor : public NonCopyable
    {
    public:
        template<typename T>
        ScopedColor(ImGuiCol col_id, T color)
        {
            ImGui::PushStyleColor(col_id, ImColor(color).Value);
        }

        ~ScopedColor() { ImGui::PopStyleColor(); }
    };

    class ScopedStyleVar : public NonCopyable
    {
    public:
        template<typename T>
        ScopedStyleVar(ImGuiStyleVar style_id, T var)
        {
            ImGui::PushStyleVar(style_id, var);
        }

        ~ScopedStyleVar() { ImGui::PopStyleVar(); }
    };
}
