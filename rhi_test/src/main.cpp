#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <app_types.hpp>
#include <cpp/containers/heap_array.hpp>
#include <events.hpp>
#include <log.hpp>
#include <pso.hpp>
#include <render/rhi.hpp>
#include <window.hpp>

#include <chrono>

#include "render/platform/vk/vk_buffer.hpp"
#include "render/platform/vk/vk_command_buffer.hpp"
#include "vk_mem_alloc.h"

static render::rhi::instance_desc get_instance_desc()
{
    constexpr auto features_table = render::rhi::rendering_features_table()
#if !defined(NDEBUG)
                                        .request(render::rhi::feature_flag::eValidation)
#endif
#if !NO_PERF_QUERY
                                        .request(render::rhi::feature_flag::ePipelineStats)
#endif
#if !defined(__APPLE__)
                                        .require(render::rhi::feature_flag::eSamplerMinMax)
#endif
                                        .request(render::rhi::feature_flag::eMeshShading)
                                        .require(render::rhi::feature_flag::e8BitIntegers)
                                        .require(render::rhi::feature_flag::e16BitTypes)
                                        .require(render::rhi::feature_flag::eDrawIndirect)
                                        .require(render::rhi::feature_flag::eDynamicRender)
                                        .require(render::rhi::feature_flag::eBindlessTextures)
                                        .require(render::rhi::feature_flag::eScalarBlockLayout)
                                        .require(render::rhi::feature_flag::eSynchronization2);
    return render::rhi::instance_desc {
        .app_name        = "GDR rhi demo",
        .app_version     = 1,
        .device_features = features_table,
    };
}

static void register_exit_callbacks(events_queue& events, bool& exit)
{
    events.add_watcher(
        event_type::key_pressed,
        [](const event_payload& payload, void* user_data)
        {
            if (payload.keyboard.key == keycode::sc_escape)
            {
                *static_cast<bool*>(user_data) = true;
            }
        },
        &exit);

    events.add_watcher(
        event_type::request_close,
        [](auto&, void* user_data)
        {
            *static_cast<bool*>(user_data) = true;
        },
        &exit);
}

static result<buffer_transfer> rhi_create_buffer_transfer(const render::rhi::rhi& rhi, render::rhi::context ctx,
                                                          render::rhi::queue_kind queue_kind, render::rhi::queue queue,
                                                          u64 staging_memory_size)
{
    const auto cmd_buffer = rhi.create_command_buffer(ctx, queue_kind);
    if (!cmd_buffer)
    {
        return error(cmd_buffer.message);
    }

    void* mapped = nullptr;
    render::rhi::create_buffer_info cbi {
        .size        = staging_memory_size,
        .usage_flags = static_cast<u32>(render::rhi::buffer_usage::eCopySrc),
        .mapped      = &mapped,
    };

    const auto staging_buffer = rhi.create_buffer(ctx, cbi);
    if (!staging_buffer)
    {
        return error(staging_buffer.message);
    }

    return buffer_transfer {
        .mapped = mapped, .queue = queue, .staging_buffer = *staging_buffer, .staging_command_buffer = *cmd_buffer};
}

static void rhi_destroy_buffer_transfer(const render::rhi::rhi& rhi, render::rhi::context ctx,
                                        buffer_transfer& buffer_transfer)
{
    buffer_transfer.mapped = nullptr;
    buffer_transfer.queue  = render::rhi::null_queue;
    rhi.destroy_buffer(ctx, buffer_transfer.staging_buffer);
    rhi.destroy_command_buffer(ctx, buffer_transfer.staging_command_buffer);
}

#define DX12_EXPERIMENTAL 0

int main(const int argc, char* argv[])
{
    logging::s_instance->set_log_level(quill::LogLevel::Debug);
    window window("RHI window", {1920, 960}, false);
    events_queue events(window);

#if DX12_EXPERIMENTAL
    auto rhi = render::rhi::create_for_dx12();
#else
    auto rhi = render::rhi::create_for_vk();
#endif

    auto context = rhi.create_context(window, get_instance_desc());
    if (!context)
    {
        LOG_ERROR("failed to create instance");
        return 1;
    }

    render::rhi::create_swapchain_info create_swapchain_info {
        .size             = window.get_size_in_px(),
        .frames_in_flight = 2,
        .format           = VK_FORMAT_R8G8B8A8_UNORM,
        .vsync            = false,
    };
    auto swapchain = rhi.create_swapchain(*context, create_swapchain_info);
    if (!swapchain)
    {
        LOG_ERROR("failed to create swapchain");
        return 1;
    }

    struct resize_context
    {
        render::rhi::rhi& rhi;
        render::rhi::swapchain& sc;
        render::rhi::context& context;
    } resize_ctx(rhi, *swapchain, *context);

    events.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& ctx = *static_cast<resize_context*>(user_data);

            const render::rhi::create_swapchain_info create_swapchain_info {
                .size             = payload.window.size_px,
                .frames_in_flight = 2,
                .format           = VK_FORMAT_B8G8R8A8_UNORM,
                .vsync            = false,
                .old_swapchain    = &ctx.sc,
            };

            if (const auto created_sc = ctx.rhi.create_swapchain(ctx.context, create_swapchain_info))
            {
                ctx.rhi.device_wait_idle(ctx.context);
                ctx.rhi.destroy_swapchain(ctx.context, ctx.sc);
                ctx.sc = *created_sc;
            }
        },
        &resize_ctx);

    const u32 cmd_count = *rhi.query_swapchain_images_count(*swapchain);
    cpp::heap_array<render::rhi::command_buffer> command_buffers(cmd_count);

    for (auto& slot : command_buffers)
    {
        const auto command_buffer = rhi.create_command_buffer(*context, render::rhi::queue_kind::eGfx);
        if (!command_buffer)
        {
            LOG_ERROR("failed to create graphics command_buffer");
            return 1;
        }

        slot = *command_buffer;
    }

    auto gfx_queue     = rhi.query_queue(*context, render::rhi::queue_kind::eGfx);
    auto copy_queue    = rhi.query_queue(*context, render::rhi::queue_kind::eTransfer);
    auto present_queue = rhi.query_queue(*context, render::rhi::queue_kind::ePresent);

    if (!gfx_queue || !present_queue || !copy_queue)
    {
        LOG_ERROR("failed to query graphics, copy or present queues, required for proper rendering");
        return 1;
    }

    auto textures_set = rhi.create_bindless_set(*context, 65536);
    if (!textures_set)
    {
        LOG_ERROR("failed to create bindless textures set");
        return 1;
    }

    pso_data pipelines;
    pipelines.load(rhi, *context, *textures_set);

    scene_geometry_pool geometry_pool {
        .vertex           = shared_buffer(rhi, *context, 128_MB, render::rhi::buffer_usage::eShaderRW),
        .meshlets         = shared_buffer(rhi, *context, 16_MB, render::rhi::buffer_usage::eShaderRW),
        .primitives       = shared_buffer(rhi, *context, 1_MB, render::rhi::buffer_usage::eShaderRW),
        .instances        = shared_buffer(rhi, *context, 48_MB, render::rhi::buffer_usage::eShaderRW),
        .materials        = shared_buffer(rhi, *context, 48_MB, render::rhi::buffer_usage::eShaderRW),
        .meshlets_payload = shared_buffer(rhi, *context, 128_MB, render::rhi::buffer_usage::eShaderRW),

        .transfer = *rhi_create_buffer_transfer(rhi, *context, render::rhi::queue_kind::eTransfer, *copy_queue, 128_MB),
    };

    auto render_loop = [&]()
    {
        auto frame_image = rhi.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image)
        {
            return;
        }

        const u32 frame_index            = *rhi.query_current_frame_index(*swapchain);
        render::rhi::command_buffer& cmd = command_buffers[frame_index];

        rhi.cmd_begin_recording(cmd);
        rhi.cmd_transition_image(cmd, *frame_image, render::rhi::image_layout::eCommon);

        render::rhi::attachment_state_info color_attachment {
            .attachment  = *frame_image,
            .load_op     = render::rhi::resource_load_op::eClear,
            .store_op    = render::rhi::resource_store_op::eStore,
            .clear_value = {},
        };

        rhi.cmd_set_draw_state(cmd,
                               {&color_attachment, 1},
                               render::rhi::null_attachment_state_info,
                               {0, 0, window.get_size_in_px().x, window.get_size_in_px().y});
        rhi.cmd_bind_pso(cmd, pipelines[pso_id::triangle]);
        rhi.cmd_draw_instanced(cmd, 3, 1, 0, 0);
        rhi.cmd_clear_draw_state(cmd);

        rhi.cmd_transition_image(cmd, *frame_image, render::rhi::image_layout::ePresent);
        rhi.cmd_end_recording(cmd);
        rhi.cmd_present_image(cmd, *swapchain, *gfx_queue, *present_queue);
        FrameMark;
    };

    bool exit = false;
    register_exit_callbacks(events, exit);

    std::function wrapper(render_loop);
    using type = decltype(wrapper);

    events.add_watcher(
        event_type::request_draw,
        [](const event_payload& payload, void* user_data)
        {
            (*static_cast<type*>(user_data))();
        },
        &wrapper);

    while (!exit)
    {
        events.poll();
    }

    rhi.device_wait_idle(*context);
    pipelines.shutdown(rhi, *context);

    rhi.destroy_buffer(*context, geometry_pool.vertex.buffer);
    rhi.destroy_buffer(*context, geometry_pool.meshlets.buffer);
    rhi.destroy_buffer(*context, geometry_pool.primitives.buffer);
    rhi.destroy_buffer(*context, geometry_pool.instances.buffer);
    rhi.destroy_buffer(*context, geometry_pool.materials.buffer);
    rhi.destroy_buffer(*context, geometry_pool.meshlets_payload.buffer);
    rhi.destroy_bindless_set(*context, *textures_set);
    rhi_destroy_buffer_transfer(rhi, *context, geometry_pool.transfer);

    for (auto& cmd : command_buffers)
    {
        rhi.destroy_command_buffer(*context, cmd);
    }
    rhi.destroy_swapchain(*context, *swapchain);
    rhi.destroy_context(*context);
    return 0;
}
