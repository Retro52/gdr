#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cpp/containers/heap_array.hpp>
#include <events.hpp>
#include <log.hpp>
#include <pso.hpp>
#include <render/rhi.hpp>
#include <window.hpp>

#include <chrono>

static rhi::instance_desc get_instance_desc()
{
    constexpr auto features_table = rhi::rendering_features_table()
#if !defined(NDEBUG)
                                        .request(rhi::feature_flag::validation)
#endif
#if !NO_PERF_QUERY
                                        .request(rhi::feature_flag::pipeline_stats)
#endif
#if !defined(__APPLE__)
                                        .require(rhi::feature_flag::sampler_min_max)
#endif
                                        .request(rhi::feature_flag::mesh_shading)
                                        .require(rhi::feature_flag::types_16bit)
                                        .require(rhi::feature_flag::draw_indirect)
                                        .require(rhi::feature_flag::dynamic_render)
                                        .require(rhi::feature_flag::bindless_textures)
                                        .require(rhi::feature_flag::scalar_block_layout)
                                        .require(rhi::feature_flag::synchronization2);
    return rhi::instance_desc {
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
    use_dx12  = use_dx12 || (argc == 2 && (strcmp(argv[1], "--d3d12") == 0));
    auto impl = use_dx12 ? rhi::create_for_d3d12() : rhi::create_for_vk();
#else
    auto impl = rhi::create_for_vk();
#endif

    auto context = impl.create_context(window, get_instance_desc());
    if (!context)
    {
        LOG_ERROR("failed to create instance. reason: {}", context.message);
        return 1;
    }

    constexpr auto kFramesInFlight  = 2;
    constexpr auto kSwapchainVsync  = false;
    constexpr auto kSwapchainFormat = rhi::image_format::r8g8b8a8un;
    rhi::create_swapchain_info create_swapchain_info {
        .size             = window.get_size_in_px(),
        .frames_in_flight = kFramesInFlight,
        .format           = kSwapchainFormat,
        .vsync            = kSwapchainVsync,
    };

    auto swapchain = impl.create_swapchain(*context, create_swapchain_info);
    if (!swapchain)
    {
        LOG_ERROR("failed to create swapchain. reason: {}", swapchain.message);
        return 1;
    }

    struct resize_context
    {
        rhi::impl& impl;
        rhi::swapchain& sc;
        rhi::context& context;
    } resize_ctx(impl, *swapchain, *context);

    events.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            if (payload.window.size_px.x < 1 || payload.window.size_px.y < 1)
            {
                return;
            }

            auto& ctx = *static_cast<resize_context*>(user_data);
            const rhi::create_swapchain_info update_swapchain_info {
                .size             = payload.window.size_px,
                .frames_in_flight = kFramesInFlight,
                .format           = kSwapchainFormat,
                .vsync            = kSwapchainVsync,
            };

            ctx.impl.device_wait_idle(ctx.context);
            if (const auto resized_sc = ctx.impl.resize_swapchain(ctx.context, ctx.sc, update_swapchain_info))
            {
                ctx.sc = *resized_sc;
            }
            else
            {
                LOG_ERROR("failed to resize swapchain. reason: {}", resized_sc.message);
            }
        },
        &resize_ctx);

    const u32 cmd_count = *impl.query_swapchain_images_count(*swapchain);
    cpp::heap_array<rhi::command_buffer> command_buffers(cmd_count);

    for (auto& slot : command_buffers)
    {
        const auto command_buffer = impl.create_command_buffer(*context, rhi::queue_kind::gfx);
        if (!command_buffer)
        {
            LOG_ERROR("failed to create graphics command_buffer");
            return 1;
        }

        slot = *command_buffer;
    }

    auto gfx_queue     = impl.query_queue(*context, rhi::queue_kind::gfx);
    auto copy_queue    = impl.query_queue(*context, rhi::queue_kind::transfer);
    auto present_queue = impl.query_queue(*context, rhi::queue_kind::present);

    if (!gfx_queue || !present_queue || !copy_queue)
    {
        LOG_ERROR("failed to query graphics, copy or present queues, required for proper rendering");
        return 1;
    }

    auto textures_set = impl.create_bindless_set(*context, 65536);
    if (!textures_set)
    {
        LOG_ERROR("failed to create bindless textures set");
        return 1;
    }

    pso_data pipelines;
    pipelines.load(impl, *context, *swapchain, *textures_set);

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

        auto frame_image = impl.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image || window.get_size_in_px().x < 1 || window.get_size_in_px().y < 1)
        {
            return;
        }

        const u32 frame_index          = *impl.query_current_frame_index(*swapchain);
        const rhi::command_buffer& cmd = command_buffers[frame_index];

        impl.cmd_begin_recording(cmd);

        {
            const rhi::image_barrier image_barrier[] = {
                {
                 .image = *frame_image,

                 .before = rhi::barrier_scope {.stages = rhi::barrier_stage::color_attachment,
                                                  .access = rhi::barrier_access::color_attachment_write},
                 .after  = rhi::barrier_scope {.stages = rhi::barrier_stage::color_attachment,
                                                  .access = rhi::barrier_access::color_attachment_read
                                                         | rhi::barrier_access::color_attachment_write},

                 .layout_after = rhi::image_layout::render_target_color,
                 .range        = rhi::image_subresource_range {.aspects = rhi::image_aspect::color},
                 }
            };

            impl.cmd_barriers(cmd, {.images = image_barrier});
        }

        rhi::attachment_state_info color_attachment {
            .attachment  = *frame_image,
            .load_op     = rhi::resource_load_op::clear,
            .store_op    = rhi::resource_store_op::store,
            .clear_value = {.color = {.f4 = vec4(0.4F, 0.6F, 0.9F, 1.0F)}},
        };

        RHI_SAFE_CALL(impl.cmd_set_draw_state,
                      cmd,
                      {&color_attachment, 1},
                      rhi::null_attachment_state_info,
                      {0, 0, window.get_size_in_px().x, window.get_size_in_px().y});
        impl.cmd_bind_pso(cmd, pipelines[pso_id::triangle]);
        impl.cmd_draw(cmd, 3, 1, 0, 0);
        impl.cmd_clear_draw_state(cmd);

        {
            const rhi::image_barrier image_barrier[] = {
                {
                 .image = *frame_image,

                 .before = rhi::barrier_scope {.stages = rhi::barrier_stage::color_attachment,
                                                  .access = rhi::barrier_access::color_attachment_write},
                 .after  = rhi::barrier_scope {.stages = rhi::barrier_stage::color_attachment,
                                                  .access = rhi::barrier_access::color_attachment_read},

                 .layout_after = rhi::image_layout::present,
                 .range        = rhi::image_subresource_range {.aspects = rhi::image_aspect::color},
                 }
            };

            impl.cmd_barriers(cmd, {.images = image_barrier});
        }

        impl.cmd_end_recording(cmd);
        impl.present(cmd, *swapchain, *gfx_queue, *present_queue);
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

    impl.device_wait_idle(*context);
    pipelines.shutdown(impl, *context);

    impl.destroy_bindless_set(*context, *textures_set);
    for (auto& cmd : command_buffers)
    {
        impl.destroy_command_buffer(*context, cmd);
    }
    impl.destroy_swapchain(*context, *swapchain);
    impl.destroy_context(*context);
    return 0;
}
