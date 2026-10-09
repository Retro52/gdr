#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <fs/path.hpp>
#include <render/rhi_handles.hpp>
#include <render/rhi_pso_options.hpp>
#include <render/types.hpp>
#include <result.hpp>
#include <window.hpp>

#include <span>

#if !defined(NDEBUG)
#include <log.hpp>
#define RHI_SAFE_CALL(FPN, ...)                         \
    [&]()                                               \
    {                                                   \
        if (FPN)                                        \
        {                                               \
            return FPN(__VA_ARGS__);                    \
        }                                               \
        LOG_WARNING("FPN '" #FPN "' is null");          \
        using return_type = decltype(FPN(__VA_ARGS__)); \
        return return_type {};                          \
    }()
#else
#define RHI_SAFE_CALL(FPN, ...) FPN(__VA_ARGS__)
#endif

namespace rhi
{
    using destroy_context_pfn = void (*)(context& context);
    using create_context_pfn  = result<context> (*)(const window& window, const instance_desc& desc);

    using destroy_swapchain_pfn = void (*)(context context, swapchain& swapchain);
    using create_swapchain_pfn  = result<swapchain> (*)(context context, const create_swapchain_info& desc);
    using resize_swapchain_pfn  = result<swapchain> (*)(context context, swapchain swapchain,
                                                       const create_swapchain_info& desc);

    using create_command_buffer_pfn  = result<command_buffer> (*)(context context, queue_kind queue_kind);
    using destroy_command_buffer_pfn = void (*)(context context, command_buffer& cmd);

    using create_bindless_set_pfn  = result<bindless_set> (*)(context context, u32 resource_count);
    using update_bindless_set_pfn  = void (*)(context context, bindless_set set,
                                             std::span<const bindless_set_write_info> info);
    using destroy_bindless_set_pfn = void (*)(context context, bindless_set& set);

    using create_fence_pfn  = result<fence> (*)(context context, u64 initial_value);
    using destroy_fence_pfn = void (*)(context context, fence& fence);

    using create_shader_pfn  = result<shader> (*)(context context, const fs::path& path);
    using destroy_shader_pfn = void (*)(context context, shader& shader);

    using create_buffer_pfn  = result<buffer> (*)(context context, const create_buffer_info& buffer_info);
    using destroy_buffer_pfn = void (*)(context context, buffer& buffer);

    using create_image_pfn  = result<image> (*)(context context, const create_image_info& image_info);
    using destroy_image_pfn = void (*)(context context, image& image);

    using create_image_view_pfn  = result<image_view> (*)(context context, image source,
                                                         const create_image_view_info& view_info);
    using destroy_image_view_pfn = void (*)(context context, image_view& view);

    using create_sampler_pfn  = result<sampler> (*)(context context, const create_sampler_info& sampler_info);
    using destroy_sampler_pfn = void (*)(context context, sampler& sampler);

    using create_compute_pso_pfn  = result<pipeline> (*)(context context, shader shader,
                                                        std::span<const bindless_set> sets);
    using create_graphics_pso_pfn = result<pipeline> (*)(context context, std::span<const shader> shaders,
                                                         std::span<const bindless_set> sets,
                                                         const pso_options& options);
    using destroy_pso_pfn         = void (*)(context context, pipeline& pso);

    using query_current_frame_index_pfn    = result<u32> (*)(swapchain swapchain);
    using query_swapchain_images_count_pfn = result<u32> (*)(swapchain swapchain);
    using query_shader_stage_pfn           = result<shader_stage> (*)(shader shader);
    using query_swapchain_color_format_pfn = result<image_format> (*)(swapchain swapchain);
    using query_feature_support_pfn        = result<bool> (*)(context context, feature_flag feature);

    using query_queue_pfn           = result<queue> (*)(context context, queue_kind kind);
    using query_device_pfn          = result<device> (*)(context context);           // kind of unused as of now
    using query_physical_device_pfn = result<physical_device> (*)(context context);  // kind of unused as of now

    using queue_wait_idle_pfn      = void (*)(queue queue);
    using device_wait_idle_pfn     = void (*)(context context);
    using fence_wait_for_value_pfn = void (*)(context context, fence fence, u64 expected_value);

    using acquire_next_swapchain_image_pfn = result<image> (*)(context context, swapchain swapchain);

    using cmd_begin_recording_pfn = void (*)(command_buffer cmd);
    using cmd_end_recording_pfn   = void (*)(command_buffer cmd);

    using cmd_reset_pfn       = void (*)(command_buffer cmd);
    using cmd_barriers_pfn    = void (*)(command_buffer cmd, const barrier_batch& barriers);
    using cmd_image_blit_pfn  = void (*)(command_buffer cmd, image src, image dst, const blit_image_info& info);
    using cmd_copy_buffer_pfn = void (*)(command_buffer cmd, buffer src_buffer, u64vec2 src_range, buffer dst_buffer,
                                         u64 dst_offset);

    using cmd_clear_buffer_pfn         = void (*)(command_buffer cmd, buffer buffer, u64vec2 range, u32 value);
    using cmd_update_buffer_pfn        = void (*)(command_buffer cmd, buffer buffer, u64vec2 range, const void* data);
    using cmd_copy_buffer_to_image_pfn = void (*)(command_buffer cmd, buffer src, image dst, image_layout layout,
                                                  std::span<const copy_image_info> regions);

    using cmd_clear_depth_attachment_pfn = void (*)(command_buffer cmd, image image, ds_clear_value value);
    using cmd_clear_color_attachment_pfn = void (*)(command_buffer cmd, image image, color_clear_value value);

    using cmd_set_draw_state_pfn   = void (*)(command_buffer cmd,
                                            std::span<const attachment_state_info> color_attachments,
                                            attachment_state_info depth_attachment, uvec4 viewport);
    using cmd_clear_draw_state_pfn = void (*)(command_buffer cmd);

    using cmd_set_cull_mode_pfn  = void (*)(command_buffer cmd, cull_mode mode);
    using cmd_set_depth_bias_pfn = void (*)(command_buffer cmd, f32 constant_factor, f32 slope_factor, f32 clamp);

    using cmd_bind_pso_pfn       = void (*)(command_buffer cmd, pipeline pso);
    using cmd_bind_index_pfn     = void (*)(command_buffer cmd, buffer index_buffer);
    using cmd_push_constants_pfn = void (*)(command_buffer cmd, pipeline pso, const void* data, u32 size, u32 offset);
    using cmd_push_bindings_pfn  = void (*)(command_buffer cmd, pipeline pso, std::span<const binding> bindings);
    using cmd_push_bindless_set_pfn = void (*)(command_buffer cmd, pipeline pso, bindless_set set, u32 binding);

    using cmd_dispatch_pfn          = void (*)(command_buffer cmd, pipeline pso, uvec3 threads);
    using cmd_dispatch_indirect_pfn = void (*)(command_buffer cmd, buffer count_buffer, u32 buffer_offset);

    using cmd_draw_pfn = void (*)(command_buffer cmd, u32 vtx_count, u32 instance_count, u32 first_vertex,
                                  u32 first_instance);

    using cmd_draw_indexed_indirect_pfn = void (*)(command_buffer cmd, buffer buffer, u64 offset, u32 count,
                                                   u32 stride);

    using cmd_draw_indexed_indirect_count_pfn = void (*)(command_buffer cmd, rhi::buffer buffer, u64 offset,
                                                         rhi::buffer count_buffer, u64 count_offset, u32 max_count,
                                                         u32 stride);

    using cmd_draw_mesh_indirect_pfn = void (*)(command_buffer cmd, buffer buffer, u64 offset, u32 count, u32 stride);

    using cmd_draw_mesh_indirect_count_pfn = void (*)(command_buffer cmd, rhi::buffer buffer, u64 offset,
                                                      rhi::buffer count_buffer, u64 count_offset, u32 max_count,
                                                      u32 stride);

    using submit_pfn  = void (*)(context context, queue queue, const submit_info& info);
    using present_pfn = void (*)(command_buffer cmd, swapchain swapchain, queue submit, queue present);

    struct impl
    {
        create_context_pfn create_context;
        destroy_context_pfn destroy_context;

        create_swapchain_pfn create_swapchain;
        resize_swapchain_pfn resize_swapchain;
        destroy_swapchain_pfn destroy_swapchain;

        create_command_buffer_pfn create_command_buffer;
        destroy_command_buffer_pfn destroy_command_buffer;

        create_bindless_set_pfn create_bindless_set;
        update_bindless_set_pfn update_bindless_set;
        destroy_bindless_set_pfn destroy_bindless_set;

        create_fence_pfn create_fence;
        destroy_fence_pfn destroy_fence;

        create_shader_pfn create_shader;
        destroy_shader_pfn destroy_shader;

        create_buffer_pfn create_buffer;
        destroy_buffer_pfn destroy_buffer;

        create_image_pfn create_image;
        destroy_image_pfn destroy_image;

        create_image_view_pfn create_image_view;
        destroy_image_view_pfn destroy_image_view;

        create_sampler_pfn create_sampler;
        destroy_sampler_pfn destroy_sampler;

        create_compute_pso_pfn create_compute_pso;
        create_graphics_pso_pfn create_graphics_pso;
        destroy_pso_pfn destroy_pso;

        query_shader_stage_pfn query_shader_stage;
        query_feature_support_pfn query_feature_support;
        query_current_frame_index_pfn query_current_frame_index;
        query_swapchain_images_count_pfn query_swapchain_images_count;
        query_swapchain_color_format_pfn query_swapchain_color_format;

        query_queue_pfn query_queue;
        query_device_pfn query_device;
        query_physical_device_pfn query_physical_device;

        queue_wait_idle_pfn queue_wait_idle;
        device_wait_idle_pfn device_wait_idle;
        fence_wait_for_value_pfn fence_wait_for_value;

        acquire_next_swapchain_image_pfn acquire_next_swapchain_image;

        cmd_begin_recording_pfn cmd_begin_recording;
        cmd_end_recording_pfn cmd_end_recording;

        cmd_reset_pfn cmd_reset;
        cmd_barriers_pfn cmd_barriers;
        cmd_image_blit_pfn cmd_image_blit;
        cmd_copy_buffer_pfn cmd_copy_buffer;

        cmd_clear_buffer_pfn cmd_clear_buffer;
        cmd_update_buffer_pfn cmd_update_buffer;
        cmd_copy_buffer_to_image_pfn cmd_copy_buffer_to_image;
        cmd_clear_depth_attachment_pfn cmd_clear_depth_attachment;
        cmd_clear_color_attachment_pfn cmd_clear_color_attachment;

        cmd_set_draw_state_pfn cmd_set_draw_state;
        cmd_clear_draw_state_pfn cmd_clear_draw_state;

        cmd_set_cull_mode_pfn cmd_set_cull_mode;
        cmd_set_depth_bias_pfn cmd_set_depth_bias;

        cmd_bind_pso_pfn cmd_bind_pso;
        cmd_bind_index_pfn cmd_bind_index;
        cmd_push_bindings_pfn cmd_push_bindings;
        cmd_push_constants_pfn cmd_push_constants;
        cmd_push_bindless_set_pfn cmd_push_bindless_set;

        cmd_dispatch_pfn cmd_dispatch;
        cmd_dispatch_indirect_pfn cmd_dispatch_indirect;

        cmd_draw_pfn cmd_draw;

        cmd_draw_indexed_indirect_pfn cmd_draw_indexed_indirect;
        cmd_draw_indexed_indirect_count_pfn cmd_draw_indexed_indirect_count;

        cmd_draw_mesh_indirect_pfn cmd_draw_mesh_indirect;
        cmd_draw_mesh_indirect_count_pfn cmd_draw_mesh_indirect_count;

        submit_pfn submit;
        present_pfn present;
    };

    impl create_for_vk();

#if GDR_ENABLE_DX12_BACKEND
    impl create_for_d3d12();
#endif
}
