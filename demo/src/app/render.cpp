#include <app/render.hpp>
#include <shaders/constants.h>
#include <tracy/Tracy.hpp>

#include <cmath>

void app::begin_rendering(const rhi::impl& rhi, rhi::command_buffer cmd, rhi::attachment color, rhi::attachment depth,
                          rhi::resource_load_op load_op, rhi::resource_store_op store_op, uvec4 viewport)
{
    ZoneScoped;

    rhi::attachment_state_info color_attachment_state {
        .attachment = color,
        .load_op    = load_op,
        .store_op   = store_op,
    };

    rhi::attachment_state_info depth_attachment_state {
        .attachment = depth,
        .load_op    = load_op,
        .store_op   = store_op,
    };

    rhi.cmd_set_draw_state(cmd, {&color_attachment_state, 1}, depth_attachment_state, viewport);
}

void app::zero_buffer(const rhi::impl& rhi, const rhi::command_buffer cmd, const rhi::buffer& draw_count_buffer,
                      const u64 offset, const u64 size)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = rhi::barrier_stage::indirect | rhi::barrier_stage::compute_shader;
#else
    constexpr auto stage_bits = rhi::barrier_stage::indirect | rhi::barrier_stage::task_shader
                              | rhi::barrier_stage::mesh_shader | rhi::barrier_stage::compute_shader;
#endif

    const rhi::buffer_barrier pre_barrier = {
        .buffer = draw_count_buffer,
        .before = {.stages = stage_bits,
                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                           | rhi::barrier_access::storage_write                                 },
        .after  = {.stages = rhi::barrier_stage::copy, .access = rhi::barrier_access::copy_write}
    };

    const rhi::buffer_barrier pre_barriers[] = {pre_barrier};
    rhi.cmd_barriers(cmd, {.buffers = pre_barriers});
    rhi.cmd_clear_buffer(cmd, draw_count_buffer, u64vec2(offset, size ? size : rhi::kBufferAll), 0);

    const rhi::buffer_barrier post_barrier = {
        .buffer = draw_count_buffer,
        .before = pre_barrier.after,
        .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect,
                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                           | rhi::barrier_access::storage_write}
    };

    const rhi::buffer_barrier post_barriers[] = {post_barrier};
    rhi.cmd_barriers(cmd, {.buffers = post_barriers});
}

void app::reset_draw_count_buffer(const rhi::impl& rhi, rhi::command_buffer cmd, const rhi::buffer& draw_count_buffer)
{
    ZoneScoped;

#ifdef __APPLE__
    constexpr auto stage_bits = rhi::barrier_stage::indirect | rhi::barrier_stage::compute_shader;
#else
    constexpr auto stage_bits = rhi::barrier_stage::indirect | rhi::barrier_stage::task_shader
                              | rhi::barrier_stage::mesh_shader | rhi::barrier_stage::compute_shader;
#endif

    const rhi::buffer_barrier pre_barrier = {
        .buffer = draw_count_buffer,
        .before = {.stages = stage_bits,
                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                           | rhi::barrier_access::storage_write                                 },
        .after  = {.stages = rhi::barrier_stage::copy, .access = rhi::barrier_access::copy_write}
    };

    const rhi::buffer_barrier pre_barriers[] = {pre_barrier};
    rhi.cmd_barriers(cmd, {.buffers = pre_barriers});

    u32 counts[shader_constants::kMatClassCount * 3];
    for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
    {
        counts[i * 3]     = 0;
        counts[i * 3 + 1] = 1;
        counts[i * 3 + 2] = 1;
    }

    rhi.cmd_update_buffer(cmd, draw_count_buffer, u64vec2(0, sizeof(u32) * COUNT_OF(counts)), counts);
    const rhi::buffer_barrier post_barrier = {
        .buffer = draw_count_buffer,
        .before = pre_barrier.after,
        .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect,
                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                           | rhi::barrier_access::storage_write}
    };

    const rhi::buffer_barrier post_barriers[] = {post_barrier};
    rhi.cmd_barriers(cmd, {.buffers = post_barriers});
}

rhi::image app::create_color_image(const rhi::impl& rhi, const ivec2& size, const rhi::image_format format,
                                   const rhi::context ctx)
{
    ZoneScoped;
    const rhi::create_image_info info {
        .format      = format,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = rhi::image_usage::sampled | rhi::image_usage::storage,
    };

    return *rhi.create_image(ctx, info);
}

rhi::image app::create_depth_image(const rhi::impl& rhi, const ivec2& size, const rhi::image_format format,
                                   const rhi::context ctx)
{
    ZoneScoped;

    const rhi::create_image_info info {
        .format      = format,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = rhi::image_usage::sampled | rhi::image_usage::transfer_dst | rhi::image_usage::attachment_ds,
    };

    return *rhi.create_image(ctx, info);
}

void app::destroy_depth_pyramid(const rhi::impl& rhi, depth_pyramid_data& pyramid, const rhi::context ctx)
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

rhi::image app::create_vis_buffer_image(const rhi::impl& rhi, const ivec2& size, const rhi::context ctx)
{
    ZoneScoped;
    const rhi::create_image_info info {
        .format      = rhi::image_format::r32g32ui,
        .dimensions  = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .usage_flags = rhi::image_usage::sampled | rhi::image_usage::transfer_src | rhi::image_usage::transfer_dst
                     | rhi::image_usage::attachment_color | rhi::image_usage::storage,
    };

    return *rhi.create_image(ctx, info);
}

app::depth_pyramid_data app::create_depth_pyramid(const rhi::impl& rhi, const ivec2& size,
                                                  const rhi::image_format format, const rhi::context ctx)
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

    const rhi::create_image_info img_info {
        .format      = format,
        .mips_count  = depth_pyramid.pyramid_count,
        .dimensions  = {depth_pyramid.base_size.x, depth_pyramid.base_size.y, 1},
        .usage_flags = rhi::image_usage::sampled | rhi::image_usage::transfer_src | rhi::image_usage::storage,
    };

    constexpr rhi::create_sampler_info sampler_info {
#ifndef __APPLE__
        .reduction = rhi::sampler_reduction::min,
#endif
        .mipmap_mode  = rhi::sampler_mipmap_mode::nearest,
        .address_mode = rhi::sampler_address_mode::clamp_to_edge,
    };

    depth_pyramid.image   = *rhi.create_image(ctx, img_info);
    depth_pyramid.sampler = *rhi.create_sampler(ctx, sampler_info);

    for (u32 i = 0; i < depth_pyramid.pyramid_count; ++i)
    {
        const rhi::create_image_view_info view_info {
            .range  = {.mips_range = {i, 1}},
            .format = format,
        };

        depth_pyramid.views[i] = *rhi.create_image_view(ctx, depth_pyramid.image, view_info);
    }

    return depth_pyramid;
}

rhi::image_barrier app::make_image_barrier(const rhi::image image, const rhi::image_layout new_layout,
                                           const rhi::image_aspect_bits aspect)
{
    ZoneScoped;
    constexpr auto kOverkillWriteAccess = rhi::barrier_access::copy_write | rhi::barrier_access::color_attachment_write
                                        | rhi::barrier_access::storage_write | rhi::barrier_access::depth_stencil_write;
    constexpr auto kOverkillReadAccess = rhi::barrier_access::copy_read | rhi::barrier_access::sampled_read
                                       | rhi::barrier_access::color_attachment_read | rhi::barrier_access::storage_read
                                       | rhi::barrier_access::depth_stencil_read;

    return make_image_barrier(image,
                              rhi::image_layout::current,
                              new_layout,
                              rhi::barrier_stage::all_commands,
                              rhi::barrier_stage::all_commands,
                              kOverkillWriteAccess,
                              kOverkillWriteAccess | kOverkillReadAccess,
                              aspect);
}

rhi::image_barrier app::make_image_barrier(const rhi::image image, const rhi::image_layout old_layout,
                                           const rhi::image_layout new_layout, const rhi::barrier_stage_bits src_stages,
                                           const rhi::barrier_stage_bits dst_stages, const rhi::barrier_access_bits src_access,
                                           const rhi::barrier_access_bits dst_access, const rhi::image_aspect_bits aspect)
{
    ZoneScoped;
    return rhi::image_barrier {
        .image         = image,
        .before        = {.stages = src_stages, .access = src_access},
        .after         = {.stages = dst_stages, .access = dst_access},
        .layout_before = old_layout,
        .layout_after  = new_layout,
        .range         = {.aspects = aspect},
    };
}
