#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cpp/containers/heap_array.hpp>
#include <events.hpp>
#include <log.hpp>
#include <pso.hpp>
#include <render/rhi.hpp>
#include <window.hpp>

#include <chrono>

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
    auto present_queue = rhi.query_queue(*context, render::rhi::queue_kind::ePresent);

    if (!gfx_queue || !present_queue)
    {
        LOG_ERROR("failed to query graphics or present queues, required for proper rendering");
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

    bool exit = false;
    register_exit_callbacks(events, exit);
    while (!exit)
    {
        events.poll();

        auto frame_image = rhi.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image)
        {
            continue;
        }

        const u32 frame_index            = *rhi.query_current_frame_index(*swapchain);
        render::rhi::command_buffer& cmd = command_buffers[frame_index];

        rhi.cmd_begin_recording(cmd);
        rhi.cmd_transition_image(cmd, *frame_image, render::rhi::image_layout::eCommon);
        rhi.cmd_transition_image(cmd, *frame_image, render::rhi::image_layout::ePresent);
        rhi.cmd_end_recording(cmd);
        rhi.cmd_present_image(cmd, *swapchain, *gfx_queue, *present_queue);
    }

    rhi.device_wait_idle(*context);
    for (auto& cmd : command_buffers)
    {
        rhi.destroy_command_buffer(*context, cmd);
    }
    rhi.destroy_swapchain(*context, *swapchain);
    rhi.destroy_context(*context);
    return 0;
}
