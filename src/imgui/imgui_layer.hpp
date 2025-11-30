#pragma once

#include <render/platform/vk/vk_renderer.hpp>
#include <scene/scene.hpp>
#include <window.hpp>

class imgui_layer
{
public:
    imgui_layer(const window& window, const render::vk_renderer& renderer);

    void begin_frame();

    void end_frame(const render::vk_renderer& renderer);
};
