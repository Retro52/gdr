#pragma once

#include <app/gpu_stats.hpp>
#include <imgui/imgui_layer.hpp>
#include <render/platform/vk/vk_geometry_pool.hpp>

struct camera_controller;

namespace editor
{
    struct info_widget_context
    {
        void draw(const app::pipeline_statistics_data& pipeline_stats) const;

        camera_controller& m_camera;
        gpu_profile_data& m_gpu_profile;
        render::vk_scene_geometry_pool& m_geometry_pool;
    };
}
