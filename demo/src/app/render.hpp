#pragma once

#include <pod_types.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct depth_pyramid_data
    {
        rhi::image image;
        rhi::sampler sampler;
        rhi::image_view views[12] {};

        ivec2 base_size;
        u32 pyramid_count {0};
    };

    void begin_rendering(const rhi::impl& rhi, rhi::command_buffer cmd, rhi::attachment color, rhi::attachment depth,
                         rhi::resource_load_op load_op, rhi::resource_store_op store_op, uvec4 viewport);

    void zero_buffer(const rhi::impl& rhi, rhi::command_buffer cmd, const rhi::buffer& draw_count_buffer,
                     u64 offset = 0, u64 size = 0);

    void reset_draw_count_buffer(const rhi::impl& rhi, rhi::command_buffer cmd, const rhi::buffer& draw_count_buffer);

    rhi::image create_color_image(const rhi::impl& rhi, const ivec2& size, rhi::image_format format, rhi::context ctx);

    rhi::image create_depth_image(const rhi::impl& rhi, const ivec2& size, rhi::image_format format, rhi::context ctx);

    void destroy_depth_pyramid(const rhi::impl& rhi, depth_pyramid_data& pyramid, rhi::context ctx);

    rhi::image create_vis_buffer_image(const rhi::impl& rhi, const ivec2& size, rhi::context ctx);

    depth_pyramid_data create_depth_pyramid(const rhi::impl& rhi, const ivec2& size, rhi::image_format format,
                                            rhi::context ctx);

    rhi::image_barrier make_image_barrier(rhi::image image, rhi::image_layout new_layout,
                                          rhi::image_aspect_bits aspect = rhi::image_aspect::color);

    rhi::image_barrier make_image_barrier(rhi::image image, rhi::image_layout old_layout, rhi::image_layout new_layout,
                                          rhi::barrier_stage_bits src_stages, rhi::barrier_stage_bits dst_stages,
                                          rhi::barrier_access_bits src_access, rhi::barrier_access_bits dst_access,
                                          rhi::image_aspect_bits aspect);
}
