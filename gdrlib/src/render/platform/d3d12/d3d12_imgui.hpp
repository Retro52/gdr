#pragma once

#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>

namespace platform
{
    void d3d12_imgui_init(d3d12_context& ctx, SDL_Window* window, u32 sc_image_count);

    void d3d12_imgui_shutdown();

    void d3d12_imgui_new_frame();

    void d3d12_imgui_draw_frame(const d3d12_command_list& cmd, const d3d12_image_view& rtv);
}
