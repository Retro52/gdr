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
                                        .request(render::rhi::feature_flag::validation)
#endif
#if !NO_PERF_QUERY
                                        .request(render::rhi::feature_flag::pipeline_stats)
#endif
#if !defined(__APPLE__)
                                        .require(render::rhi::feature_flag::sampler_min_max)
#endif
                                        .request(render::rhi::feature_flag::mesh_shading)
                                        .require(render::rhi::feature_flag::types_16bit)
                                        .require(render::rhi::feature_flag::draw_indirect)
                                        .require(render::rhi::feature_flag::dynamic_render)
                                        .require(render::rhi::feature_flag::bindless_textures)
                                        .require(render::rhi::feature_flag::scalar_block_layout)
                                        .require(render::rhi::feature_flag::synchronization2);
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

#define DX12_EXPERIMENTAL         0
#define RHI_TEXTURES_EXPERIMENTAL 0

int main(const int argc, char* argv[])
{
#if TRACY_ENABLE
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif

    ZoneScoped;
    logging::s_instance->set_log_level(quill::LogLevel::Debug);
    window window("RHI window", {.fullscreen = true, .borderless = true});

    events_queue events(window);

#if DX12_EXPERIMENTAL
    bool use_dx12 = true;
#else
    bool use_dx12 = false;
#endif

#if GDR_ENABLE_DX12_BACKEND
    use_dx12 = use_dx12 || (argc == 2 && (strcmp(argv[1], "--d3d12") == 0));
    auto rhi = use_dx12 ? render::rhi::create_for_d3d12() : render::rhi::create_for_vk();
#else
    auto rhi = render::rhi::create_for_vk();
#endif

    auto context = rhi.create_context(window, get_instance_desc());
    if (!context)
    {
        LOG_ERROR("failed to create instance. reason: {}", context.message);
        return 1;
    }

    constexpr auto kFramesInFlight  = 2;
    constexpr auto kSwapchainVsync  = false;
    constexpr auto kSwapchainFormat = render::rhi::image_format::r8g8b8a8un;
    render::rhi::create_swapchain_info create_swapchain_info {
        .size             = window.get_size_in_px(),
        .frames_in_flight = kFramesInFlight,
        .format           = kSwapchainFormat,
        .vsync            = kSwapchainVsync,
    };

    auto swapchain = rhi.create_swapchain(*context, create_swapchain_info);
    if (!swapchain)
    {
        LOG_ERROR("failed to create swapchain. reason: {}", swapchain.message);
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
            if (payload.window.size_px.x < 1 || payload.window.size_px.y < 1)
            {
                return;
            }

            auto& ctx = *static_cast<resize_context*>(user_data);
            const render::rhi::create_swapchain_info update_swapchain_info {
                .size             = payload.window.size_px,
                .frames_in_flight = kFramesInFlight,
                .format           = kSwapchainFormat,
                .vsync            = kSwapchainVsync,
            };

            ctx.rhi.device_wait_idle(ctx.context);
            if (const auto resized_sc = ctx.rhi.resize_swapchain(ctx.context, ctx.sc, update_swapchain_info))
            {
                ctx.sc = *resized_sc;
            }
            else
            {
                LOG_ERROR("failed to resize swapchain. reason: {}", resized_sc.message);
            }
        },
        &resize_ctx);

    const u32 cmd_count = *rhi.query_swapchain_images_count(*swapchain);
    cpp::heap_array<render::rhi::command_buffer> command_buffers(cmd_count);

    for (auto& slot : command_buffers)
    {
        const auto command_buffer = rhi.create_command_buffer(*context, render::rhi::queue_kind::gfx);
        if (!command_buffer)
        {
            LOG_ERROR("failed to create graphics command_buffer");
            return 1;
        }

        slot = *command_buffer;
    }

    auto gfx_queue     = rhi.query_queue(*context, render::rhi::queue_kind::gfx);
    auto copy_queue    = rhi.query_queue(*context, render::rhi::queue_kind::transfer);
    auto present_queue = rhi.query_queue(*context, render::rhi::queue_kind::present);

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
    pipelines.load(rhi, *context, *swapchain, *textures_set);

    auto get_time = []<typename T = f64>()
    {
        return static_cast<T>(SDL_GetPerformanceCounter()) / static_cast<T>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    auto render_loop    = [&]
    {
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto frame_image = rhi.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image || window.get_size_in_px().x < 1 || window.get_size_in_px().y < 1)
        {
            return;
        }

        const u32 frame_index                  = *rhi.query_current_frame_index(*swapchain);
        const render::rhi::command_buffer& cmd = command_buffers[frame_index];

        rhi.cmd_begin_recording(cmd);

        {
            const render::rhi::image_barrier image_barrier[] = {
                {
                 .image = *frame_image,

                 .before =
                        render::rhi::barrier_scope {
                            .stages = static_cast<u32>(render::rhi::barrier_stage::color_attachment),
                            .access = static_cast<u32>(render::rhi::barrier_access::color_attachment_write)},
                 .after = render::rhi::barrier_scope {.stages = static_cast<u32>(
                                                             render::rhi::barrier_stage::color_attachment),
                                                         .access = render::rhi::barrier_access::color_attachment_read
                                                                 | render::rhi::barrier_access::color_attachment_write},

                 .layout_after = render::rhi::image_layout::render_target_color,
                 .range        = render::rhi::image_subresource_range {.aspects = static_cast<u32>(
                                                                       render::rhi::image_aspect::color)},
                 }
            };

            rhi.cmd_barriers(cmd, {.images = image_barrier});
        }

        render::rhi::attachment_state_info color_attachment {
            .attachment  = *frame_image,
            .load_op     = render::rhi::resource_load_op::clear,
            .store_op    = render::rhi::resource_store_op::store,
            .clear_value = {.color = {.f4 = vec4(0.4F, 0.6F, 0.9F, 1.0F)}},
        };

        RHI_SAFE_CALL(rhi.cmd_set_draw_state,
                      cmd,
                      {&color_attachment, 1},
                      render::rhi::null_attachment_state_info,
                      {0, 0, window.get_size_in_px().x, window.get_size_in_px().y});
        rhi.cmd_bind_pso(cmd, pipelines[pso_id::triangle]);
        rhi.cmd_draw(cmd, 3, 1, 0, 0);
        rhi.cmd_clear_draw_state(cmd);

        {
            const render::rhi::image_barrier image_barrier[] = {
                {
                 .image = *frame_image,

                 .before =
                        render::rhi::barrier_scope {
                            .stages = static_cast<u32>(render::rhi::barrier_stage::color_attachment),
                            .access = static_cast<u32>(render::rhi::barrier_access::color_attachment_write)},
                 .after =
                        render::rhi::barrier_scope {
                            .stages = static_cast<u32>(render::rhi::barrier_stage::color_attachment),
                            .access = static_cast<u32>(render::rhi::barrier_access::color_attachment_read)},

                 .layout_after = render::rhi::image_layout::present,
                 .range        = render::rhi::image_subresource_range {.aspects = static_cast<u32>(
                                                                       render::rhi::image_aspect::color)},
                 }
            };

            rhi.cmd_barriers(cmd, {.images = image_barrier});
        }

        rhi.cmd_end_recording(cmd);
        rhi.present(cmd, *swapchain, *gfx_queue, *present_queue);
        SDL_SetWindowTitle(
            window.get_native_handle().window,
            cpp::stack_string::make_formatted("CPU time: %lfms; FPS: %lf", dt * 1000.0F, 1.0F / dt).c_str());
        FrameMark;
    };

    bool exit = false;
    register_exit_callbacks(events, exit);

    std::function wrapper(render_loop);
    using type = decltype(wrapper);

#if 1
    events.add_watcher(
        event_type::request_draw,
        [](const event_payload& payload, void* user_data)
        {
            (*static_cast<type*>(user_data))();
        },
        &wrapper);
#endif

    while (!exit)
    {
        events.poll();
    }

    rhi.device_wait_idle(*context);
    pipelines.shutdown(rhi, *context);

    rhi.destroy_bindless_set(*context, *textures_set);
    for (auto& cmd : command_buffers)
    {
        rhi.destroy_command_buffer(*context, cmd);
    }
    rhi.destroy_swapchain(*context, *swapchain);
    rhi.destroy_context(*context);
    return 0;
}
