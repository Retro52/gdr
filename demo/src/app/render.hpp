#pragma once

#include <pod_types.hpp>
#include <render/rhi.hpp>

namespace app
{
    struct depth_pyramid_data
    {
        render::rhi::image image;
        render::rhi::sampler sampler;
        render::rhi::image_view views[12] {};

        ivec2 base_size;
        u32 pyramid_count {0};
    };

    void begin_rendering(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd, render::rhi::attachment color,
                         render::rhi::attachment depth, render::rhi::resource_load_op load_op,
                         render::rhi::resource_store_op store_op, uvec4 viewport);

    void zero_buffer(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd,
                     const render::rhi::buffer& draw_count_buffer, u64 offset = 0, u64 size = 0);

    void reset_draw_count_buffer(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd,
                                 const render::rhi::buffer& draw_count_buffer);

    render::rhi::image create_color_image(const render::rhi::rhi& rhi, const ivec2& size,
                                          render::rhi::image_format format, render::rhi::context ctx);

    render::rhi::image create_depth_image(const render::rhi::rhi& rhi, const ivec2& size,
                                          render::rhi::image_format format, render::rhi::context ctx);

    void destroy_depth_pyramid(const render::rhi::rhi& rhi, depth_pyramid_data& pyramid, render::rhi::context ctx);

    render::rhi::image create_vis_buffer_image(const render::rhi::rhi& rhi, const ivec2& size,
                                               render::rhi::context ctx);

    depth_pyramid_data create_depth_pyramid(const render::rhi::rhi& rhi, const ivec2& size,
                                            render::rhi::image_format format, render::rhi::context ctx);

    render::rhi::image_barrier make_image_barrier(
        render::rhi::image image, render::rhi::image_layout new_layout,
        render::rhi::image_aspects aspect = static_cast<u32>(render::rhi::image_aspect::color));

    render::rhi::image_barrier make_image_barrier(render::rhi::image image, render::rhi::image_layout old_layout,
                                                  render::rhi::image_layout new_layout,
                                                  render::rhi::barrier_stages src_stages,
                                                  render::rhi::barrier_stages dst_stages,
                                                  render::rhi::barrier_accesses src_access,
                                                  render::rhi::barrier_accesses dst_access,
                                                  render::rhi::image_aspects aspect);
}
