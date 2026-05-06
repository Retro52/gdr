#pragma once

#include <pod_types.hpp>
#include <render/platform/vk/vk_query.hpp>

namespace app
{
    struct pipeline_statistics_data
    {
        u64 input_assembly_vertices     = 0;
        u64 input_assembly_primitives   = 0;
        u64 vertex_shader_invocations   = 0;
        u64 triangles_count             = 0;
        u64 fragment_shader_invocations = 0;
    };

    struct frame_statistics_data
    {
        u64 frame_start = 0;
        u64 frame_end   = 0;
    };

    frame_statistics_data query_frame_statistics_data(VkDevice device, render::vk_query query);
    pipeline_statistics_data query_pipeline_statistics_data(VkDevice device, render::vk_query query);
}
