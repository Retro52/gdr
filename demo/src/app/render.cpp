#include <app/render.hpp>
#include <shaders/constants.h>
#include <tracy/Tracy.hpp>

#include <cmath>

void app::begin_rendering(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd, render::rhi::attachment color,
                          render::rhi::attachment depth, render::rhi::resource_load_op load_op,
                          render::rhi::resource_store_op store_op, uvec4 viewport)
{
    ZoneScoped;

    render::rhi::attachment_state_info color_attachment_state {
        .attachment = color,
        .load_op    = load_op,
        .store_op   = store_op,
    };

    render::rhi::attachment_state_info depth_attachment_state {
        .attachment = depth,
        .load_op    = load_op,
        .store_op   = store_op,
    };

    rhi.cmd_set_draw_state(cmd, {&color_attachment_state, 1}, depth_attachment_state, viewport);
}

void app::zero_buffer(const render::rhi::rhi& rhi, const render::rhi::command_buffer cmd,
                      const render::rhi::buffer& draw_count_buffer, const u64 offset, const u64 size)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = render::rhi::barrier_stage::indirect | render::rhi::barrier_stage::compute_shader;
#else
    constexpr auto stage_bits = render::rhi::barrier_stage::indirect | render::rhi::barrier_stage::task_shader
                              | render::rhi::barrier_stage::mesh_shader | render::rhi::barrier_stage::compute_shader;
#endif

    const render::rhi::buffer_barrier pre_barrier = {
        .buffer = draw_count_buffer,
        .before = {.stages = stage_bits,
                   .access = render::rhi::barrier_access::indirect_read | render::rhi::barrier_access::storage_read
                           | render::rhi::barrier_access::storage_write               },
        .after  = {.stages = static_cast<u32>(render::rhi::barrier_stage::copy),
                   .access = static_cast<u32>(render::rhi::barrier_access::copy_write)}
    };

    const render::rhi::buffer_barrier pre_barriers[] = {pre_barrier};
    rhi.cmd_barriers(cmd, {.buffers = pre_barriers});
    rhi.cmd_clear_buffer(cmd, draw_count_buffer, u64vec2(offset, size ? size : render::rhi::kBufferAll), 0);

    const render::rhi::buffer_barrier post_barrier = {
        .buffer = draw_count_buffer,
        .before = pre_barrier.after,
        .after  = {.stages = render::rhi::barrier_stage::compute_shader | render::rhi::barrier_stage::indirect,
                   .access = render::rhi::barrier_access::indirect_read | render::rhi::barrier_access::storage_read
                           | render::rhi::barrier_access::storage_write}
    };

    const render::rhi::buffer_barrier post_barriers[] = {post_barrier};
    rhi.cmd_barriers(cmd, {.buffers = post_barriers});
}

void app::reset_draw_count_buffer(const render::rhi::rhi& rhi, render::rhi::command_buffer cmd,
                                  const render::rhi::buffer& draw_count_buffer)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = render::rhi::barrier_stage::indirect | render::rhi::barrier_stage::compute_shader;
#else
    constexpr auto stage_bits = render::rhi::barrier_stage::indirect | render::rhi::barrier_stage::task_shader
                              | render::rhi::barrier_stage::mesh_shader | render::rhi::barrier_stage::compute_shader;
#endif

    const render::rhi::buffer_barrier pre_barrier = {
        .buffer = draw_count_buffer,
        .before = {.stages = stage_bits,
                   .access = render::rhi::barrier_access::indirect_read | render::rhi::barrier_access::storage_read
                           | render::rhi::barrier_access::storage_write               },
        .after  = {.stages = static_cast<u32>(render::rhi::barrier_stage::copy),
                   .access = static_cast<u32>(render::rhi::barrier_access::copy_write)}
    };

    const render::rhi::buffer_barrier pre_barriers[] = {pre_barrier};
    rhi.cmd_barriers(cmd, {.buffers = pre_barriers});

    u32 counts[shader_constants::kMatClassCount * 3];
    for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
    {
        counts[i * 3]     = 0;
        counts[i * 3 + 1] = 1;
        counts[i * 3 + 2] = 1;
    }

    rhi.cmd_update_buffer(cmd, draw_count_buffer, u64vec2(0, sizeof(u32) * COUNT_OF(counts)), counts);
    const render::rhi::buffer_barrier post_barrier = {
        .buffer = draw_count_buffer,
        .before = pre_barrier.after,
        .after  = {.stages = render::rhi::barrier_stage::compute_shader | render::rhi::barrier_stage::indirect,
                   .access = render::rhi::barrier_access::indirect_read | render::rhi::barrier_access::storage_read
                           | render::rhi::barrier_access::storage_write}
    };

    const render::rhi::buffer_barrier post_barriers[] = {post_barrier};
    rhi.cmd_barriers(cmd, {.buffers = post_barriers});
}

render::rhi::image app::create_color_image(const render::rhi::rhi& rhi, const ivec2& size,
                                           const render::rhi::image_format format, const render::rhi::context ctx)
{
    ZoneScoped;
    const render::rhi::create_image_info info {
        .format      = format,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = render::rhi::image_usage::sampled | render::rhi::image_usage::storage,
    };

    return *rhi.create_image(ctx, info);
}

render::rhi::image app::create_depth_image(const render::rhi::rhi& rhi, const ivec2& size,
                                           const render::rhi::image_format format, const render::rhi::context ctx)
{
    ZoneScoped;

    const render::rhi::create_image_info info {
        .format      = format,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = render::rhi::image_usage::sampled | render::rhi::image_usage::transfer_dst
                     | render::rhi::image_usage::attachment_ds,
    };

    return *rhi.create_image(ctx, info);
}

void app::destroy_depth_pyramid(const render::rhi::rhi& rhi, depth_pyramid_data& pyramid,
                                const render::rhi::context ctx)
{
    ZoneScoped;
    for (u32 i = 0; i < pyramid.pyramid_count; ++i)
    {
        rhi.destroy_image_view(ctx, pyramid.views[i]);
    }

    pyramid.pyramid_count = 0;
    rhi.destroy_image(ctx, pyramid.image);
    rhi.destroy_sampler(ctx, pyramid.sampler);
}

render::rhi::image app::create_vis_buffer_image(const render::rhi::rhi& rhi, const ivec2& size,
                                                const render::rhi::context ctx)
{
    ZoneScoped;
    const render::rhi::create_image_info info {
        .format      = render::rhi::image_format::r32g32ui,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = render::rhi::image_usage::sampled | render::rhi::image_usage::transfer_src
                     | render::rhi::image_usage::transfer_dst | render::rhi::image_usage::attachment_color
                     | render::rhi::image_usage::storage,
    };

    return *rhi.create_image(ctx, info);
}

app::depth_pyramid_data app::create_depth_pyramid(const render::rhi::rhi& rhi, const ivec2& size,
                                                  const render::rhi::image_format format,
                                                  const render::rhi::context ctx)
{
    ZoneScoped;
    depth_pyramid_data depth_pyramid {
        .base_size     = {1 << static_cast<i32>(std::log2(size.x)), 1 << static_cast<i32>(std::log2(size.y))},
        .pyramid_count = 1,
    };

    ivec2 size_cpy = depth_pyramid.base_size;
    while (size_cpy.x > 1 || size_cpy.y > 1)
    {
        size_cpy /= 2;
        ++depth_pyramid.pyramid_count;
    }

    depth_pyramid.pyramid_count =
        std::min(depth_pyramid.pyramid_count, static_cast<u32>(COUNT_OF(depth_pyramid.views)));

    const render::rhi::create_image_info img_info {
        .format      = format,
        .mips_count  = depth_pyramid.pyramid_count,
        .dimensions  = {depth_pyramid.base_size.x, depth_pyramid.base_size.y, 1},
        .usage_flags = render::rhi::image_usage::sampled | render::rhi::image_usage::transfer_src
                     | render::rhi::image_usage::storage,
    };

    constexpr render::rhi::create_sampler_info sampler_info {
#ifndef __APPLE__
        .reduction = render::rhi::sampler_reduction::min,
#endif
        .mipmap_mode  = render::rhi::sampler_mipmap_mode::nearest,
        .address_mode = render::rhi::sampler_address_mode::clamp_to_edge,
    };

    depth_pyramid.image   = *rhi.create_image(ctx, img_info);
    depth_pyramid.sampler = *rhi.create_sampler(ctx, sampler_info);

    for (u32 i = 0; i < depth_pyramid.pyramid_count; ++i)
    {
        const render::rhi::create_image_view_info view_info {
            .range  = {.mips_range = {i, 1}},
            .format = format,
        };

        depth_pyramid.views[i] = *rhi.create_image_view(ctx, depth_pyramid.image, view_info);
    }

    return depth_pyramid;
}

render::rhi::image_barrier app::make_image_barrier(const render::rhi::image image,
                                                   const render::rhi::image_layout new_layout,
                                                   const render::rhi::image_aspects aspect)
{
    ZoneScoped;
    constexpr u32 kOverkillWriteAccess =
        render::rhi::barrier_access::copy_write | render::rhi::barrier_access::color_attachment_write
        | render::rhi::barrier_access::storage_write | render::rhi::barrier_access::depth_stencil_write;
    constexpr u32 kOverkillReadAccess =
        render::rhi::barrier_access::copy_read | render::rhi::barrier_access::sampled_read
        | render::rhi::barrier_access::color_attachment_read | render::rhi::barrier_access::storage_read
        | render::rhi::barrier_access::depth_stencil_read;

    return make_image_barrier(image,
                              render::rhi::image_layout::current,
                              new_layout,
                              static_cast<u32>(render::rhi::barrier_stage::all_commands),
                              kOverkillWriteAccess,
                              static_cast<u32>(render::rhi::barrier_stage::all_commands),
                              kOverkillWriteAccess | kOverkillReadAccess,
                              aspect);
}

render::rhi::image_barrier app::make_image_barrier(
    const render::rhi::image image, const render::rhi::image_layout old_layout,
    const render::rhi::image_layout new_layout, const render::rhi::barrier_stages src_stages,
    const render::rhi::barrier_stages dst_stages, const render::rhi::barrier_accesses src_access,
    const render::rhi::barrier_accesses dst_access, const render::rhi::image_aspects aspect)
{
    ZoneScoped;
    return render::rhi::image_barrier {
        .image         = image,
        .before        = {.stages = src_stages, .access = src_access},
        .after         = {.stages = dst_stages, .access = dst_access},
        .layout_before = old_layout,
        .layout_after  = new_layout,
        .range         = {.aspects = aspect},
    };
}
