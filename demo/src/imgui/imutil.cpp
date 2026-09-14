#include <imgui/imex.hpp>
#include <imgui/imutil.hpp>
#include <imgui.h>

void ImGuiUtil::TextCenteredDisabled(const char* text)
{
    const f32 w = ImGui::CalcTextSize(text).x;
    HorizontalCentering(w);
    ImGui::TextDisabled("%s", text);
}

void ImGuiUtil::VerticalCentering(f32 block_height)
{
    const f32 avail = ImGui::GetContentRegionAvail().y;
    if (avail > block_height)
    {
        ImGui::Dummy({0, (avail - block_height) * 0.5f});
    }
}

void ImGuiUtil::HorizontalCentering(f32 item_width)
{
    const f32 avail = ImGui::GetContentRegionAvail().x;
    if (avail > item_width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - item_width) * 0.5f);
    }
}
