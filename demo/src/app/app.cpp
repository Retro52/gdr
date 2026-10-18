#include <types.hpp>

#include <app/app.hpp>
#include <app/csm.hpp>
#include <app/envmap.hpp>
#include <app/gpu_upload_mgr.hpp>
#include <app/pso.hpp>
#include <app/render.hpp>
#include <camera_controller.hpp>
#include <events.hpp>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <log.hpp>
#include <passes/pass_aa.hpp>
#include <passes/pass_dbg_frustum.hpp>
#include <passes/pass_geometry.hpp>
#include <passes/pass_light_resolve.hpp>
#include <passes/pass_post_process.hpp>
#include <passes/pass_present.hpp>
#include <render_settings.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/loader.hpp>
#include <scene/mesh_load.hpp>
#include <scene/scene.hpp>
#include <tracy/Tracy.hpp>
#include <window.hpp>

#define NO_POPULATE_MODE 1

#define RESULT_EXIT_IF_FAILED(result)                                       \
    if (!result) [[unlikely]]                                               \
    {                                                                       \
        LOG_CRITICAL("exiting due to a critical error: {}", result.message) \
        return 1;                                                           \
    }

namespace
{
    REGISTER_ENUM(debug_mode, shaded, lit, lit_diffuse, lit_ambient, lit_specular, uv, normal, tangent, depth, velocity,
                  world_pos, color, metallic, roughness, albedo_texture, normal_texture, orm_texture, triangle_id,
                  instance_id, triangle_face);

    struct frames_tracker
    {
        struct frame_data
        {
            rhi::command_buffer command_buffer = rhi::null_command_buffer;
        };

        u32 frame_index = 0;
        cpp::heap_array<frame_data> frame_data;

        void next_frame() { frame_index = (frame_index + 1) % frame_data.size(); }

        [[nodiscard]] rhi::command_buffer get_frame_command_buffer() const noexcept
        {
            return frame_data[frame_index].command_buffer;
        }

        template<typename Func>
        void submit(Func&& func)
        {
            std::invoke(func, get_frame_command_buffer());
        }

        void init(const rhi::impl& rhi, const rhi::context ctx, const u32 frames_in_flight)
        {
            frame_data.resize(frames_in_flight);
            for (auto& frame : frame_data)
            {
                frame.command_buffer = *rhi.create_command_buffer(ctx, rhi::queue_kind::gfx);
            }
        }
    };

    void build_frustum(shader_types::FrameCullData& data, const glm::mat4& iproj, const glm::mat4& iview)
    {
        ZoneScoped;

        data.view          = iview;
        data.p00           = iproj[0][0];
        data.p11           = iproj[1][1];
        data.lod_threshold = 2.0F / (data.viewport_size.y * glm::abs(data.p11));

        auto t_pv = glm::transpose(iproj);

        auto plane = [&](const vec4 eq)
        {
            return eq / glm::length(vec3(eq));
        };

        const vec4 hor_plane = plane(t_pv[3] + t_pv[0]);
        const vec4 ver_plane = plane(t_pv[3] + t_pv[1]);

        data.frustum[0] = hor_plane.x;
        data.frustum[1] = hor_plane.z;

        data.frustum[2] = glm::abs(ver_plane.y);
        data.frustum[3] = ver_plane.z;

        const auto w = t_pv[2].w;
        const auto z = glm::max(t_pv[2].z, 1e-9F);

        data.frustum[4] = w - z;
        data.frustum[5] = w / z;
    }

    template<typename T>
    T get_random(const T min, const T max)
    {
        ZoneScoped;
        return min + (static_cast<T>(rand()) / RAND_MAX) * (max - min);
    }

#if !NO_POPULATE_MODE
    loader::scene_info populate_scene(const u32 draw_count, const cpp::heap_array<mesh::raw_mesh>& primitives,
                                      scene& scene, platform::vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

        const u32 kVolumeItemsPerSide = std::lroundl(std::cbrt(draw_count));

        loader::loader_context ctx;
        cpp::heap_array<loader::prim_layout> layouts(primitives.size());

        u64 total_vertices = 0;
        u64 total_meshlets = 0;
        u64 total_payload  = 0;
        for (u32 p = 0; p < primitives.size(); ++p)
        {
            auto& layout         = layouts[p];
            layout.prim_index    = p + geometry_pool.primitives.offset / sizeof(loader::primitive);
            layout.vertex_offset = total_vertices + (geometry_pool.vertex.offset / sizeof(loader::vertex));

            total_vertices += primitives[p].raw_vertices.size();

            for (u32 i = 0; i < primitives[p].lod_count; ++i)
            {
                const auto& lod     = primitives[p].lod_array[i];
                layout.lod_array[i] = {total_meshlets + (geometry_pool.meshlets.offset / sizeof(loader::meshlet)),
                                       total_payload + geometry_pool.meshlets_payload.offset};

                total_meshlets += lod.raw_meshlets.size();
                total_payload += lod.raw_meshlets_payload.size();
            }
        }

        ctx.primitives.resize(primitives.size());
        ctx.vertices.resize(total_vertices);
        ctx.meshlets.resize(total_meshlets);
        ctx.meshlets_data.resize(total_payload);

        ctx.materials.resize(draw_count);
        cpp::heap_array<loader::instance> instances(draw_count);

        for (u32 i = 0; i < primitives.size(); ++i)
        {
            loader::encode_raw_mesh(ctx, primitives[i], layouts[i]);
        }

        u64 triangles_max     = 0;
        u64 visibility_offset = 0;

        std::array<u32, shader_constants::kMatClassCount> mat_offset_table {};
        for (u32 i = 0; i < draw_count; ++i)
        {
            ZoneScopedN("create models within the scene");

            const u32 id_random = get_random<i32>(0, primitives.size() - 1);

            auto entity = scene.create_entity();
            entity.add_component<id_component>();

            auto& instance = instances[i];
            vec3 position  = {
                i % kVolumeItemsPerSide,
                (i / kVolumeItemsPerSide) % kVolumeItemsPerSide,
                i / (kVolumeItemsPerSide * kVolumeItemsPerSide),
            };

            position *= vec3(1.5F);

#if 0
            constexpr f32 kDensityInverse = 7.5F;
            position *= vec3(get_random<f32>(-kDensityInverse, kDensityInverse),
                             get_random<f32>(-kDensityInverse, kDensityInverse),
                             get_random<f32>(-kDensityInverse, kDensityInverse));

            instance.pos_and_scale = {position, get_random<f32>(0.75F, 10.0F)};
            instance.rotation_quat =
                glm::quat(vec3(get_random<f32>(-180, 180), get_random<f32>(-180, 180), get_random<f32>(-180, 180)));
#else
            instance.pos_and_scale = {position, 0.25F};
            instance.rotation_quat = glm::quat(vec3());
#endif

            instance.material_index    = i;
            instance.visibility_offset = visibility_offset;
            instance.mesh_data_index   = layouts[id_random].prim_index;
            instance.base_vertex       = ctx.primitives[id_random].base_vertex;

            auto& material          = ctx.materials[i];
            material.material_class = shader_constants::kMatClassOpaque;
            material.diffuse_factor = vec4(1.0F, 0.0F, 0.0F, 1.0);

            const f32 kMixMax = static_cast<f32>(kVolumeItemsPerSide) - 1;
            material.met_roughness_factor.g =
                glm::mix(0.0F, 1.0F, static_cast<f32>((i / kVolumeItemsPerSide) % kVolumeItemsPerSide) / kMixMax);
            material.met_roughness_factor.b = glm::mix(0.0F, 1.0F, static_cast<f32>(i % kVolumeItemsPerSide) / kMixMax);

            triangles_max += loader::get_max_lod_tris(primitives[id_random]);
            visibility_offset += loader::get_max_lod_meshlets(ctx.primitives[id_random]);

            mat_offset_table[material.material_class] +=
                (loader::get_max_lod_meshlets(ctx.primitives[id_random]) + shader_constants::kTaskWorkGroups - 1)
                / shader_constants::kTaskWorkGroups;
        }

        platform::vk_upload_data(
            geometry_pool.transfer, geometry_pool.primitives, ctx.primitives.data(), ctx.primitives.size());
        platform::vk_upload_data(
            geometry_pool.transfer, geometry_pool.vertex, ctx.vertices.data(), ctx.vertices.size());
        platform::vk_upload_data(
            geometry_pool.transfer, geometry_pool.meshlets, ctx.meshlets.data(), ctx.meshlets.size());
        platform::vk_upload_data(
            geometry_pool.transfer, geometry_pool.materials, ctx.materials.data(), ctx.materials.size());
        platform::vk_upload_data(
            geometry_pool.transfer, geometry_pool.meshlets_payload, ctx.meshlets_data.data(), ctx.meshlets_data.size());
        platform::vk_upload_data(geometry_pool.transfer, geometry_pool.instances, instances.data(), instances.size());

        return {.meshes           = primitives.size(),
                .meshlets         = visibility_offset,
                .triangles        = triangles_max,
                .primitives       = draw_count,
                .mat_offset_table = mat_offset_table};
    }
#endif
}

// dumb order of initialization issue here, but what can u do
// this function was only supposed to actually create the renderer, but now it will also apply some CLI args
static rhi::impl load_rhi(const app::argv_handler& args)
{
    ZoneScoped;

    if (const auto ll = args.read_numeric("--log_level", -1); ll != -1)
    {
        logging::s_instance->set_log_level(static_cast<quill::LogLevel>(ll));
    }

    const bool use_dx12 = args.read_numeric("--use_dx12");

#if GDR_ENABLE_DX12_BACKEND
    return use_dx12 ? rhi::create_for_d3d12() : rhi::create_for_vk();
#else
    return rhi::create_for_vk();
#endif
}

static rhi::instance_desc get_instance_desc(auto& args)
{
    constexpr auto features_table = rhi::rendering_features_table()
#if !defined(NDEBUG)
                                        .request(rhi::feature_flag::validation)
                                        .request(rhi::feature_flag::debug_labels)
#endif
#if !defined(__APPLE__)
                                        .require(rhi::feature_flag::sampler_min_max)
#endif
                                        .request(rhi::feature_flag::mesh_shading)
                                        .require(rhi::feature_flag::types_16bit)
                                        .require(rhi::feature_flag::types_int8)
                                        .require(rhi::feature_flag::draw_indirect)
                                        .require(rhi::feature_flag::dynamic_render)
                                        .require(rhi::feature_flag::bindless_textures)
                                        .require(rhi::feature_flag::scalar_block_layout)
                                        .require(rhi::feature_flag::synchronization2);
    return rhi::instance_desc {
        .app_name        = "GDR",
        .device_id_hint  = static_cast<u32>(args.read_numeric("--device_id", rhi::kDeviceIdDealersChoice)),
        .device_features = features_table,
    };
}

static window create_app_window()
{
    return window {
        "GDR", {.position = {200, 200}, .fullscreen = false}
    };
}

app::instance::instance(const int argc, char* argv[])
    : m_window(create_app_window())
    , m_args(argc, argv)
    , m_rhi(load_rhi(m_args))
    , m_events_queue(m_window)
{
}

int app::instance::run()
{
    if (m_args.argc() < 2)
    {
        LOG_ERROR("No arguments specified, forgot to pass gltf scene?");
        return -1;
    }

    auto context = m_rhi.create_context(m_window, get_instance_desc(m_args));
    RESULT_EXIT_IF_FAILED(context);

    constexpr auto kFramesInFlight  = 2;
    constexpr auto kSwapchainVsync  = false;
    constexpr auto kSwapchainFormat = rhi::image_format::r8g8b8a8un;
    rhi::create_swapchain_info create_swapchain_info {
        .size             = m_window.get_size_in_px(),
        .frames_in_flight = kFramesInFlight,
        .format           = kSwapchainFormat,
        .vsync            = kSwapchainVsync,
    };

    auto swapchain = m_rhi.create_swapchain(*context, create_swapchain_info);
    RESULT_EXIT_IF_FAILED(swapchain);

    common_resources resources(m_rhi, *context, kFramesInFlight);

    pso_data pipelines;
    pipelines.load(m_rhi, *context, *swapchain, resources.bindless_textures);

    pso_watcher watcher(pipelines, m_rhi, *context, resources.bindless_textures);

    app::pass_present present_pass(m_rhi, *context);
    app::pass_light_resolve lpass(m_rhi, *context, m_window.get_size_in_px());
    app::pass_geometry gpass(m_rhi, pipelines, *context, m_window.get_size_in_px());

    app::pass_aa aa_pass(m_rhi, pipelines, *context, m_window.get_size_in_px(), m_window.get_size_in_px());
    app::pass_post_process pp_pass(m_rhi, *context, m_window.get_size_in_px());

    app::pass_dbg_frustum dbg_frustum_pass(m_rhi, *context, pipelines[pso_id::frustum_debug]);

    app::envmap envmap {
        m_rhi,
        *context,
        rhi::image_format::r16g16b16a16sf,
        {.env_resolution = 1024, .brdf_lut_resolution = 512, .prefilter_resolution = 128, .irradiance_resolution = 32}
    };

    app::csm csm {
        m_rhi, *context, rhi::image_format::d32sf, {.resolution = 2048, .max_range = 500.0F, .split_lambda = 0.65F}
    };

    bool exit                   = false;
    bool mesh_shading_supported = *m_rhi.query_feature_support(*context, rhi::feature_flag::mesh_shading);

    m_events_queue.add_watcher(
        event_type::request_close,
        [](auto&, void* user_data)
        {
            *static_cast<bool*>(user_data) = true;
        },
        &exit);

    m_events_queue.add_watcher(
        event_type::key_pressed,
        [](const event_payload& payload, void* user_data)
        {
            if (payload.keyboard.key == keycode::sc_escape)
            {
                *static_cast<bool*>(user_data) = true;
            }
        },
        &exit);

    struct resize_context
    {
        rhi::impl& rhi;
        rhi::context& context;
        rhi::swapchain& swapchain;

        app::pass_aa& aa_pass;
        app::pass_geometry& gpass;
        app::pass_light_resolve& lpass;
        app::pass_post_process& pp_pass;
    } resize_ctx(m_rhi,
                 *context,
                 *swapchain,

                 aa_pass,
                 gpass,
                 lpass,
                 pp_pass);

    m_events_queue.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& ctx = *static_cast<resize_context*>(user_data);

            ctx.rhi.device_wait_idle(ctx.context);

            const rhi::create_swapchain_info update_swapchain_info {
                .size             = payload.window.size_px,
                .frames_in_flight = kFramesInFlight,
                .format           = kSwapchainFormat,
                .vsync            = kSwapchainVsync,
            };

            ctx.swapchain = *ctx.rhi.resize_swapchain(ctx.context, ctx.swapchain, update_swapchain_info);

            ctx.lpass.on_render_size_changed(payload.window.size_px);
            ctx.gpass.on_render_size_changed(payload.window.size_px);
            ctx.pp_pass.on_display_size_changed(payload.window.size_px);
            ctx.aa_pass.on_render_display_size_changed(payload.window.size_px, payload.window.size_px);
        },
        &resize_ctx);

    gpu_upload_mgr upload_mgr(m_rhi, *context, 128_MB);

    loader::scene_counters scene_counters;

    const int instance_count = m_args.read_numeric("--instances");
    const int first_instance = m_args.get_positional_args_start();
    auto env_map             = m_args.read_string<fs::path_string>("--skymap");

    assert2(instance_count == 0 || first_instance > 0);

    // test scene stuff
    scene client_scene;
    cpp::heap_array<rhi::image> textures;
#if !NO_POPULATE_MODE
    if (instance_count > 0 && first_instance > 0)
    {
        cpp::heap_array<mesh::raw_mesh> meshes;
        for (int i = first_instance; i < m_args.argc(); ++i)
        {
            auto ctx = loader::load_meshes(m_args.argv()[i]);
            if (!ctx)
            {
                continue;
            }

            meshes.append(ctx->primitives);
        }

        scene_counters = populate_scene(instance_count, meshes, client_scene, geometry_pool);
    }
    else
#endif
    {
        ZoneScopedN("loader::load_scene");
        const auto scene_data = *loader::load_scene(m_args.argv()[first_instance], client_scene);
        scene_counters        = scene_data.counters;

        cpp::local_array<rhi::bindless_set_write_info> updates;
        for (u32 i = 0; i < scene_data.textures.size(); ++i)
        {
            const auto handle = upload_mgr.create_texture(scene_data.textures[i]);
            textures.emplace_back(handle);
            updates.push_back(rhi::bindless_set_write_info {.dst = handle, .index = i + 1});
        }

        m_rhi.update_bindless_set(*context, resources.bindless_textures, std::span {updates});

        upload_mgr.submit(resources.vertex, scene_data.vertices.data(), scene_data.vertices.size());
        upload_mgr.submit(resources.meshlets, scene_data.meshlets.data(), scene_data.meshlets.size());
        upload_mgr.submit(resources.instances, scene_data.instances.data(), scene_data.instances.size());
        upload_mgr.submit(resources.materials, scene_data.materials.data(), scene_data.materials.size());
        upload_mgr.submit(resources.primitives, scene_data.primitives.data(), scene_data.primitives.size());
        upload_mgr.submit(resources.meshlets_payload, scene_data.meshlets_data.data(), scene_data.meshlets_data.size());
    }

    entity camera = client_scene.empty();
    if (const auto loaded_camera = client_scene.get_view<entt::entity, camera_component>().front();
        loaded_camera != entt::null)
    {
        camera = client_scene.create_ref(loaded_camera);
    }

    entity editor_camera = client_scene.create_entity();
    editor_camera.add_component<id_component>(DEBUG_ONLY(id_component("editor camera")));
    editor_camera.add_component<transform_component>(transform_component {vec3(0, 1, 5), 1.0F, glm::quat()});
    editor_camera.add_component<camera_component>(camera_component {
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(60.0F),
    });

    if (!camera)
    {
        camera = editor_camera;
    }

    entity sun = client_scene.empty();
    if (const auto loaded_sun = client_scene.get_view<entt::entity, directional_light_component>().front();
        loaded_sun != entt::null)
    {
        sun = client_scene.create_ref(loaded_sun);
    }
    else
    {
        sun = client_scene.create_entity();
        sun.add_component<id_component>(DEBUG_ONLY(id_component("sun")));
        sun.add_component<directional_light_component>(vec3(1), 5500.0F);

        auto& t    = sun.emplace_component<transform_component>();
        t.rotation = glm::quat(vec3(0, 1, 0));
    }

    sun.get_component<transform_component>().rotation = glm::quat(glm::radians(m_args.read_vec3(
        "--sun_direction", glm::degrees(glm::eulerAngles(sun.get_component<transform_component>().rotation)))));

    camera.get_component<transform_component>().rotation = glm::quat(glm::radians(m_args.read_vec3(
        "--camera_direction", glm::degrees(glm::eulerAngles(camera.get_component<transform_component>().rotation)))));

    camera.get_component<transform_component>().position =
        m_args.read_vec3("--camera_position", camera.get_component<transform_component>().position);

    render_settings client_render_settings;

    glm::mat4 camera_proj;
    glm::mat4 camera_proj_jittered;

    glm::mat4 camera_view;
    glm::mat4 debug_camera_view;
    glm::mat4 mv_pref_frame_view_proj;

    debug_mode draw_debug_mode {debug_mode::shaded};

    f32 camera_exposure        = 10.0F;
    f32 envmap_compensation_ev = 0.0f;
    f32 envmap_intensity       = 1250.0f;

    bool enable_fsr_aa            = true;
    bool fsr_aa_rcas              = false;
    bool fsr_aa_reset             = false;
    bool freeze_cull_data         = false;
    bool enable_meshlets_pipeline = mesh_shading_supported;

    struct keyboard_controls_context
    {
        bool& fsr_aa_rcas;
        bool& fsr_aa_reset;
        bool& enable_fsr_aa;
        bool& freeze_cull_data;
        bool& enable_meshlets_pipeline;

        debug_mode& draw_debug_mode;
    } keyboard_controls_ctx(
        fsr_aa_rcas, fsr_aa_reset, enable_fsr_aa, freeze_cull_data, enable_meshlets_pipeline, draw_debug_mode);

    m_events_queue.add_watcher(
        event_type::key_pressed,
        [](const event_payload& payload, void* user_data)
        {
            if (payload.keyboard.key == keycode::m)
            {
                bool& var = static_cast<keyboard_controls_context*>(user_data)->enable_meshlets_pipeline;
                var       = !var;

                LOG_INFO("Mesh shading is now: {}", var ? "enabled" : "disabled");
            }

            if (payload.keyboard.key == keycode::r)
            {
                bool& var = static_cast<keyboard_controls_context*>(user_data)->fsr_aa_reset;
                var       = true;
                LOG_INFO("FSR reset");
            }
            if (payload.keyboard.key == keycode::t)
            {
                bool& var = static_cast<keyboard_controls_context*>(user_data)->enable_fsr_aa;
                var       = !var;

                static_cast<keyboard_controls_context*>(user_data)->fsr_aa_reset = true;
                LOG_INFO("FSR is now: {}", var ? "enabled" : "disabled");
            }
            if (payload.keyboard.key == keycode::y)
            {
                bool& var = static_cast<keyboard_controls_context*>(user_data)->fsr_aa_rcas;
                var       = !var;

                LOG_INFO("FSR RCAS is now: {}", var ? "enabled" : "disabled");
            }

            if (payload.keyboard.key == keycode::f)
            {
                bool& var = static_cast<keyboard_controls_context*>(user_data)->freeze_cull_data;
                var       = !var;

                LOG_INFO("Freeze cull data is now: {}", var ? "enabled" : "disabled");
            }
            if (payload.keyboard.key == keycode::sc_up)
            {
                debug_mode& var = static_cast<keyboard_controls_context*>(user_data)->draw_debug_mode;
                var             = static_cast<debug_mode>(cpp::min(
                    static_cast<u32>(var) + 1, static_cast<u32>(reflection::get_enum_values_count<debug_mode>() - 1)));

                LOG_INFO("New debug view mode: {}", reflection::get_enum_name_at<debug_mode>(static_cast<u32>(var)));
            }
            if (payload.keyboard.key == keycode::sc_down)
            {
                debug_mode& var = static_cast<keyboard_controls_context*>(user_data)->draw_debug_mode;
                var             = static_cast<debug_mode>(cpp::max(static_cast<u32>(var), 1U) - 1);

                LOG_INFO("New debug view mode: {}", reflection::get_enum_name_at<debug_mode>(static_cast<u32>(var)));
            }
        },
        &keyboard_controls_ctx);

    auto get_time = []<typename T = f64>()
    {
        return static_cast<T>(SDL_GetPerformanceCounter()) / static_cast<T>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    camera_controller controller(m_events_queue, camera);

    frames_tracker tracker;
    tracker.init(m_rhi, *context, *m_rhi.query_swapchain_images_count(*swapchain));

    {
        ZoneScopedN("app.instance.run.preload");
        auto fence = *m_rhi.create_fence(*context, 0);
        auto cmd   = *m_rhi.create_command_buffer(*context, rhi::queue_kind::gfx);

        m_rhi.cmd_begin_recording(cmd);
        csm.init(m_rhi, cmd);

        envmap.init(m_rhi, cmd, pipelines);
        if (!env_map.empty())
        {
            envmap.load(m_rhi, cmd, pipelines, upload_mgr, env_map);
        }
        m_rhi.cmd_end_recording(cmd);

        const rhi::fence_submit_info signals[] = {
            {.fence = fence, .value = 1}
        };

        const rhi::submit_info submit_info {
            .signals         = signals,
            .command_buffers = {&cmd, 1},
        };
        m_rhi.submit(*context, *m_rhi.query_queue(*context, rhi::queue_kind::gfx), submit_info);
        m_rhi.fence_wait_for_value(*context, fence, 1);
        m_rhi.destroy_fence(*context, fence);
        m_rhi.destroy_command_buffer(*context, cmd);
    }

    auto render_loop = [&]()
    {
        ZoneScopedN("app.instance.run.render_loop");
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update(static_cast<f32>(dt));

        const auto frame_image = m_rhi.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image)
        {
            return;
        }

        tracker.next_frame();
        resources.next_frame();
        tracker.submit(
            [&](const rhi::command_buffer buffer)
            {
                ZoneScopedN("main.m_renderer.submit");

                m_rhi.cmd_begin_recording(buffer);
                auto& frame_cull_data_buffer = resources.frame_cull_data_buffer();
                if (!freeze_cull_data)
                {
                    auto projection = client_render_settings.render_distance > 0
                                        ? camera_data.get_projection_matrix(client_render_settings.render_distance)
                                        : camera_data.get_projection_matrix();

                    auto view = camera_component::get_view_matrix(camera_transform.position, camera_transform.rotation);
                    debug_camera_view = view;

                    shader_types::FrameCullData fcd {.pyramid_size  = gpass.hizb.base_size,
                                                     .viewport_size = m_window.get_size_in_px(),
                                                     .draw_count    = static_cast<u32>(scene_counters.instances),
                                                     .flags         = client_render_settings.flags};
                    build_frustum(fcd, projection, view);
                    (*static_cast<shader_types::FrameCullData*>(frame_cull_data_buffer.mapped)) = fcd;
                }
                else
                {
                    static_cast<shader_types::FrameCullData*>(frame_cull_data_buffer.mapped)->view = debug_camera_view;
                    static_cast<shader_types::FrameCullData*>(frame_cull_data_buffer.mapped)->flags =
                        client_render_settings.flags;
                }

                camera_proj = camera_data.get_projection_matrix();
                camera_view = camera_component::get_view_matrix(camera_transform.position, camera_transform.rotation);

                auto& sun_transform = sun.get_component<transform_component>();
                auto& sun_data      = sun.get_component<directional_light_component>();

                auto sun_direction = glm::normalize(glm::mat3_cast(sun_transform.rotation) * vec3(0, 0, 1));

                auto& world_data_buffer                                                 = resources.world_data_buffer();
                (*static_cast<shader_types::FrameWorldData*>(world_data_buffer.mapped)) = shader_types::FrameWorldData {
                    .sun_color         = {sun_data.rgb_color, sun_data.intensity},
                    .camera_pos        = camera_transform.position,
                    .debug_mode        = static_cast<u32>(draw_debug_mode),
                    .sun_direction     = sun_direction,
                    .camera_exposure   = camera_exposure,
                    .envmap_scale      = envmap_intensity * glm::exp2(envmap_compensation_ev),
                    .render_resolution = m_window.get_size_in_px(),
                };

                aa_pass.next_jitter();
                aa_pass.backend      = enable_fsr_aa ? aa_backend::fsr3 : aa_backend::fxaa;
                camera_proj_jittered = aa_pass.get_jittered_matrix(camera_proj);

                auto& motion_vectors_buffer = resources.motion_vector_data_buffer();
                (*static_cast<shader_types::MotionVectorsData*>(motion_vectors_buffer.mapped)) =
                    shader_types::MotionVectorsData {
                        .inv_frame_vp  = glm::inverse(camera_proj_jittered * camera_view),
                        .this_frame_vp = camera_proj * camera_view,
                        .prev_frame_vp = mv_pref_frame_view_proj,
                    };
                mv_pref_frame_view_proj = camera_proj * camera_view;

                gpass.draw(
                    buffer,
                    resources,
                    scene_counters,
                    pass_geometry::draw_params {
                        .view       = freeze_cull_data ? debug_camera_view : camera_view,
                        .projection = camera_proj_jittered,
                        .flags = enable_meshlets_pipeline ? gpass_flags::pre_hizb_mesh : gpass_flags::pre_hizb_indexed,
                    });

                if (freeze_cull_data)
                {
                    gpass.draw(buffer,
                               resources,
                               scene_counters,
                               pass_geometry::draw_params {
                                   .view       = camera_view,
                                   .projection = camera_proj_jittered,
                                   .flags      = (enable_meshlets_pipeline ? gpass_flags::pre_hizb_mesh
                                                                           : gpass_flags::pre_hizb_indexed)
                                          & ~gpass_flags::build_hizb,
                               });
                }

                gpass.draw(buffer,
                           resources,
                           scene_counters,
                           pass_geometry::draw_params {
                               .view       = camera_view,
                               .projection = camera_proj_jittered,
                               .flags      = enable_meshlets_pipeline ? gpass_flags::post_hizb_mesh
                                                                      : gpass_flags::post_hizb_indexed,
                           });

                pass_light_resolve::draw_params light_params {
                    .color         = gpass.v_buffer,
                    .depth         = gpass.depth,
                    .indices       = gpass.indices,
                    .pipeline      = enable_meshlets_pipeline ? pipelines[pso_id::mesh_resolve_pipeline]
                                                              : pipelines[pso_id::vert_resolve_pipeline],
                    .brdf_lookup   = envmap.get_lut_descriptor_info(),
                    .env_cube      = envmap.get_cube_descriptor_info(),
                    .env_conv      = envmap.get_conv_descriptor_info(),
                    .env_prefilter = envmap.get_pref_descriptor_info(),
                    .view          = camera_view,
                    .projection    = camera_proj_jittered,
                };
                lpass.draw(buffer, resources, light_params);

                const pass_aa::fsr3_params fsr3_params {
                    .mv_matrices = resources.motion_vector_data_buffer().buffer,
                    .ffx_upscale =
                        {
                                      .reset            = fsr_aa_reset,
                                      .reverse_z        = true,
                                      .enable_sharpness = fsr_aa_rcas,
                                      .sharpness        = 1.0F,

                                      .dt_ms              = static_cast<f32>(dt * 1000.0F),
                                      .camera_far_plane   = kF32Max,
                                      .camera_near_plane  = camera_data.near_plane,
                                      .camera_fov_radians = camera_data.horizontal_fov,
                                      },
                };

                pass_aa::draw_params params {
                    .color = lpass.output,
                    .depth = gpass.depth,
                };

                switch (aa_pass.backend)
                {
                case aa_backend::fsr3 :
                    params.params.ffx = fsr3_params;
                    break;
                case aa_backend::fxaa :
                    params.params.fxaa = {.camera_near = camera_data.near_plane};
                    break;
                default :
                    break;
                }

                fsr_aa_reset = false;
                aa_pass.draw(buffer, params);

                pass_post_process::draw_params post_params {
                    .color    = aa_pass.output,
                    .pipeline = pipelines[pso_id::tonemap_pipeline],
                };

                pp_pass.draw(buffer, post_params);

                if (freeze_cull_data)
                {
                    dbg_frustum_pass.draw(buffer,
                                          app::pass_dbg_frustum::draw_params {
                                              .cull_data = frame_cull_data_buffer.buffer,
                                              .depth     = gpass.depth,
                                              .output    = pp_pass.output,
                                              .viewport  = m_window.get_size_in_px(),
                                              .camera_vp = camera_proj * camera_view,
                                          });
                }

                present_pass.present(buffer,
                                     *swapchain,
                                     {
                                         .color       = pp_pass.output,
                                         .output      = *frame_image,
                                         .color_res   = m_window.get_size_in_px(),
                                         .display_res = m_window.get_size_in_px(),
                                     });

                static f64 accumulated_time = 0.0;
                accumulated_time            = accumulated_time * 0.95 + dt * 0.05;
                const auto str              = cpp::stack_string::make_formatted(
                    "[%s] CPU: %.3lfms",
                    reflection::get_enum_name_at<debug_mode>(static_cast<u32>(draw_debug_mode)),
                    accumulated_time * 1000.0F);
                SDL_SetWindowTitle(m_window.get_native_handle().window, str.c_str());

                FrameMark;
            });
    };

    std::function wrapper(render_loop);
    m_events_queue.add_watcher(
        event_type::request_draw,
        [](auto&, void* user_data)
        {
            std::invoke(*static_cast<std::function<void()>*>(user_data));
        },
        &wrapper);

    while (!exit)
    {
        m_events_queue.poll();
    }

    m_rhi.device_wait_idle(*context);

    lpass.shutdown();
    gpass.shutdown();
    aa_pass.shutdown();
    pp_pass.shutdown();

    csm.shutdown(m_rhi, *context);
    envmap.shutdown(m_rhi, *context);

    watcher.shutdown();
    pipelines.shutdown(m_rhi, *context);

    m_rhi.destroy_buffer(*context, resources.vertex);
    m_rhi.destroy_buffer(*context, resources.meshlets);
    m_rhi.destroy_buffer(*context, resources.primitives);
    m_rhi.destroy_buffer(*context, resources.instances);
    m_rhi.destroy_buffer(*context, resources.materials);
    m_rhi.destroy_buffer(*context, resources.meshlets_payload);

    m_rhi.destroy_sampler(*context, resources.bindless_sampler);
    m_rhi.destroy_bindless_set(*context, resources.bindless_textures);

    for (auto& buffer : resources.world_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }
    for (auto& buffer : resources.frame_cull_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }
    for (auto& buffer : resources.motion_vector_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }

    for (auto& texture : textures)
    {
        m_rhi.destroy_image(*context, texture);
    }

    return 0;
}
