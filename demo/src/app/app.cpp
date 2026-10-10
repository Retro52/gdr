#include <volk.h>

#include <types.hpp>

#include <app/app.hpp>
#include <app/csm.hpp>
#include <app/envmap.hpp>
#include <app/gpu_upload_mgr.hpp>
#include <app/pso.hpp>
#include <app/render.hpp>
#include <camera_controller.hpp>
#include <codegen/render_settings.hpp>
#include <debug/frustum_renderer.hpp>
#include <editor/hierarchy.hpp>
#include <editor/info.hpp>
#include <events.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <log.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/loader.hpp>
#include <scene/mesh_load.hpp>
#include <scene/scene.hpp>
#include <shaders/bindings/draw.h>
#include <shaders/bindings/fill.h>
#include <shaders/bindings/shadow_cull.h>
#include <shaders/bindings/shadow_draw.h>
#include <tracy/Tracy.hpp>
#include <window.hpp>

#define NO_EDITOR        1
#define NO_PERF_QUERY    1
#define NO_POPULATE_MODE 1

#if !NO_PERF_QUERY
#include <app/gpu_stats.hpp>
#endif

#define RESULT_EXIT_IF_FAILED(result)                                       \
    if (!result) [[unlikely]]                                               \
    {                                                                       \
        LOG_CRITICAL("exiting due to a critical error: {}", result.message) \
        return 1;                                                           \
    }

#if !NO_EDITOR
#include <imgui.h>
#include <imgui/gpu_profile_data.hpp>
#include <imgui/imex.hpp>
#include <imgui/imgui_layer.hpp>
#include <imgui/imwidgets.hpp>
#endif

namespace
{
    REGISTER_ENUM(debug_mode, shaded, lit, lit_diffuse, lit_ambient, lit_specular, uv, normal, tangent, depth,
                  world_pos, color, metallic, roughness, albedo_texture, normal_texture, omr_texture, triangle_id,
                  instance_id, triangle_face, shadow, shadow_cascades);

    REGISTER_ENUM(cubemap_face, XP, XN, YP, YN, ZP, ZN);

    struct world_geometry
    {
        rhi::buffer vertex;
        rhi::buffer meshlets;
        rhi::buffer primitives;
        rhi::buffer instances;
        rhi::buffer materials;
        rhi::buffer meshlets_payload;
    };

    struct descriptor_bindings
    {
        constexpr static u32 kMaxSetZeroBindings = 32;
        u32 max_count                            = 0;
        rhi::binding bindings[kMaxSetZeroBindings] {};

        std::span<rhi::binding> get() { return {bindings, max_count}; }

        auto& bind_at(const rhi::binding& next, const u32 index)
        {
            assert2(index < kMaxSetZeroBindings);
            bindings[index] = next;
            max_count       = cpp::min(kMaxSetZeroBindings, cpp::max(max_count, index + 1));
            return *this;
        }
    };

    struct frames_tracker
    {
        using delete_callback_t = std::function<void(const rhi::impl& rhi, rhi::context ctx)>;

        struct frame_data
        {
            rhi::command_buffer command_buffer = rhi::null_command_buffer;
            cpp::heap_array<delete_callback_t> delete_callbacks;
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

static std::array<u32, shader_constants::kMatClassCount> make_offset_table(
    const std::array<u32, shader_constants::kMatClassCount>& materials)
{
    std::array<u32, shader_constants::kMatClassCount> result {};

    u32 sum = 0;
    for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
    {
        result[i] = sum;
        sum += materials[i];
    }

    return result;
}

static const rhi::pipeline& get_shadow_pipeline(app::pso_data& pipelines, const u32 material_class,
                                                const bool enable_meshlets)
{
    app::pso_id id {};
    switch (material_class)
    {
    case shader_constants::kMatClassMasked :
    case shader_constants::kMatClassTranslucent :

    {
        id = enable_meshlets ? app::pso_id::shadow_draw_task_ds : app::pso_id::shadow_draw_indexed_ds;
        break;
    }
    default :
    case shader_constants::kMatClassOpaque :
    {
        id = enable_meshlets ? app::pso_id::shadow_draw_task_ss : app::pso_id::shadow_draw_indexed_ss;
        break;
    }
    }

    return pipelines[id];
}

static const rhi::pipeline& get_render_pipeline(app::pso_data& pipelines, const u32 material_class,
                                                const bool occlusion_cull, const bool enable_meshlets)
{
    app::pso_id id {};
    switch (material_class)
    {
    case shader_constants::kMatClassMasked :
    case shader_constants::kMatClassTranslucent :

    {
        id = enable_meshlets
               ? (occlusion_cull ? app::pso_id::task_render_ds_late_pipeline : app::pso_id::task_render_ds_pipeline)
               : app::pso_id::indexed_render_ds_pipeline;
        break;
    }
    default :
    case shader_constants::kMatClassOpaque :
    {
        id = enable_meshlets
               ? (occlusion_cull ? app::pso_id::task_render_late_pipeline : app::pso_id::task_render_pipeline)
               : app::pso_id::indexed_render_pipeline;
        break;
    }
    }

    return pipelines[id];
}

static void app_cmd_transition_image(const rhi::impl& rhi, const rhi::command_buffer cmd, const rhi::image image,
                                     const rhi::image_layout layout,
                                     const rhi::image_aspect_bits aspects = rhi::image_aspect::color)
{
    const rhi::image_barrier barrier = app::make_image_barrier(image, layout, aspects);

    const rhi::barrier_batch barriers {
        .images = {&barrier, 1}
    };

    rhi.cmd_barriers(cmd, barriers);
}

static std::array<glm::mat4, shader_constants::kMaxShadowCascades> update_csm_buffers(
    const app::csm& csm, const app::mapped_buffer& csm_buffer, const vec3& light_dir, const camera_component& camera,
    const glm::mat4& camera_view, const vec3& camera_pos, const render_settings& settings)
{
    const auto light_view      = csm.get_light_view_matrix(light_dir);
    const auto camera_relative = glm::translate(glm::mat4(1.0F), camera_pos);

    shader_types::ShadowCascadesData& shadow_data = *static_cast<shader_types::ShadowCascadesData*>(csm_buffer.mapped);
    shadow_data.depth_bias                        = settings.shadow_shader_bias;
    shadow_data.normal_offset                     = settings.shadow_normal_offset;
    shadow_data.blend_ratio                       = glm::clamp(settings.shadow_cascade_blend, 0.0F, 1.0F);
    shadow_data.max_range                         = csm.max_range;
    shadow_data.view                              = light_view;
    shadow_data.sm_resolution                     = static_cast<f32>(csm.resolution);

    std::array<glm::mat4, shader_constants::kMaxShadowCascades> result {};

    for (u32 i = 0; i < shader_constants::kMaxShadowCascades; ++i)
    {
        const auto cascade_ivp =
            csm.get_cascade_inv_vp(camera.near_plane, camera.aspect_ratio, camera.horizontal_fov, camera_view, i);
        const auto sphere = csm.get_cascade_sphere(cascade_ivp);
        const auto bounds = csm.get_cascade_bounds(sphere, light_view);

        result[i] =
            glm::orthoRH_ZO(bounds.min.x, bounds.max.x, bounds.max.y, bounds.min.y, -bounds.min.z, -bounds.max.z);
        result[i] *= light_view;

        shader_types::CascadeData& data = shadow_data.cascades[i];
        data.bounds                     = bounds;
        data.vp                         = result[i] * camera_relative;
        data.split                      = csm.get_cascade_range(camera.near_plane, i);
        data.texel_world_size           = 2.0F * sphere.w / static_cast<f32>(csm.resolution);
    }

    return result;
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
#endif
#if !NO_PERF_QUERY
                                        .request(rhi::feature_flag::ePipelineStats)
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
        .app_version     = 1,
        .device_id_hint  = static_cast<u32>(args.read_numeric("--device_id", -1)),
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
    constexpr auto kSwapchainVsync  = true;
    constexpr auto kSwapchainFormat = rhi::image_format::r8g8b8a8un;
    rhi::create_swapchain_info create_swapchain_info {
        .size             = m_window.get_size_in_px(),
        .frames_in_flight = kFramesInFlight,
        .format           = kSwapchainFormat,
        .vsync            = kSwapchainVsync,
    };

    auto swapchain = m_rhi.create_swapchain(*context, create_swapchain_info);
    RESULT_EXIT_IF_FAILED(swapchain);

    rhi::image depth_image = create_depth_image(m_rhi, m_window.get_size_in_px(), rhi::image_format::d32sf, *context);

    rhi::image render_target =
        create_color_image(m_rhi, m_window.get_size_in_px(), rhi::image_format::r8g8b8a8un, *context);

    rhi::image vis_buffer = create_vis_buffer_image(m_rhi, m_window.get_size_in_px(), *context);

    depth_pyramid_data depth_pyramid =
        create_depth_pyramid(m_rhi, m_window.get_size_in_px(), rhi::image_format::r32sf, *context);

    app::envmap envmap {
        m_rhi,
        *context,
        rhi::image_format::r16g16b16a16sf,
        {.env_resolution = 1024, .brdf_lut_resolution = 512, .prefilter_resolution = 128, .irradiance_resolution = 32}
    };

    app::csm csm {
        m_rhi, *context, rhi::image_format::d32sf, {.resolution = 2048, .max_range = 500.0F, .split_lambda = 0.65F}
    };

    bool exit                     = false;
    bool mesh_shading_supported   = *m_rhi.query_feature_support(*context, rhi::feature_flag::mesh_shading);
    bool pipeline_stats_supported = *m_rhi.query_feature_support(*context, rhi::feature_flag::pipeline_stats);

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

        rhi::image& vis_buffer;
        rhi::image& depth_image;
        rhi::image& render_target;
        depth_pyramid_data& depth_pyramid;
    } resize_ctx(m_rhi, *context, *swapchain, vis_buffer, depth_image, render_target, depth_pyramid);

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
            ctx.rhi.destroy_image(ctx.context, ctx.depth_image);
            ctx.rhi.destroy_image(ctx.context, ctx.render_target);

            ctx.depth_image =
                create_depth_image(ctx.rhi, payload.window.size_px, rhi::image_format::d32sf, ctx.context);

            ctx.render_target =
                create_color_image(ctx.rhi, payload.window.size_px, rhi::image_format::r8g8b8a8un, ctx.context);

            destroy_depth_pyramid(ctx.rhi, ctx.depth_pyramid, ctx.context);
            ctx.depth_pyramid =
                create_depth_pyramid(ctx.rhi, payload.window.size_px, rhi::image_format::r32sf, ctx.context);

            ctx.rhi.destroy_image(ctx.context, ctx.vis_buffer);
            ctx.vis_buffer = create_vis_buffer_image(ctx.rhi, payload.window.size_px, ctx.context);
        },
        &resize_ctx);

    auto bindless_textures_desc_set = m_rhi.create_bindless_set(*context, 65536);
    auto bindless_textures_sampler  = m_rhi.create_sampler(*context,
                                                          rhi::create_sampler_info {
                                                               .anisotropy_factor = 16,
                                                               .dbg_name          = "bindless_textures_sampler",
                                                          });

    auto shadow_alpha_sampler = m_rhi.create_sampler(*context,
                                                     rhi::create_sampler_info {
                                                         .mipmap_mode = rhi::sampler_mipmap_mode::nearest,
                                                         .dbg_name    = "shadow_alpha_sampler",
                                                     });

    auto color_sampler = m_rhi.create_sampler(*context, {.mipmap_mode = rhi::sampler_mipmap_mode::nearest});

    auto depth_texture_sampler = m_rhi.create_sampler(*context,
                                                      {
                                                          .filter       = rhi::sampler_filter::nearest,
                                                          .mipmap_mode  = rhi::sampler_mipmap_mode::nearest,
                                                          .address_mode = rhi::sampler_address_mode::clamp_to_border,
                                                          .dbg_name     = "depth_texture_sampler",
                                                      });

    pso_data pipelines;
    pipelines.load(m_rhi, *context, *swapchain, *bindless_textures_desc_set);
    pso_watcher watcher(pipelines, m_rhi, *context, *bindless_textures_desc_set);

#if !NO_EDITOR
    imgui_layer editor(m_window, m_renderer, pipelines);
#endif

    gpu_upload_mgr upload_mgr(m_rhi, *context, 128_MB);
    world_geometry geometry_pool {
        .vertex           = *m_rhi.create_buffer(*context, {.size = 128_MB, .dbg_name = "vertex"}),
        .meshlets         = *m_rhi.create_buffer(*context, {.size = 16_MB, .dbg_name = "meshlets"}),
        .primitives       = *m_rhi.create_buffer(*context, {.size = 1_MB, .dbg_name = "primitives"}),
        .instances        = *m_rhi.create_buffer(*context, {.size = 48_MB, .dbg_name = "instances"}),
        .materials        = *m_rhi.create_buffer(*context, {.size = 48_MB, .dbg_name = "materials"}),
        .meshlets_payload = *m_rhi.create_buffer(*context, {.size = 128_MB, .dbg_name = "meshlets_payload"}),
    };

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

        m_rhi.update_bindless_set(*context, *bindless_textures_desc_set, std::span {updates});

        upload_mgr.submit(geometry_pool.vertex, scene_data.vertices.data(), scene_data.vertices.size());
        upload_mgr.submit(geometry_pool.meshlets, scene_data.meshlets.data(), scene_data.meshlets.size());
        upload_mgr.submit(geometry_pool.instances, scene_data.instances.data(), scene_data.instances.size());
        upload_mgr.submit(geometry_pool.materials, scene_data.materials.data(), scene_data.materials.size());
        upload_mgr.submit(geometry_pool.primitives, scene_data.primitives.data(), scene_data.primitives.size());
        upload_mgr.submit(
            geometry_pool.meshlets_payload, scene_data.meshlets_data.data(), scene_data.meshlets_data.size());
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

#if !NO_PERF_QUERY
    constexpr u32 kQueryPoolCount = 64;
    platform::vk_query timestamp_query_pool =
        *platform::vk_create_query_pool(m_renderer.get_context().device, kQueryPoolCount, VK_QUERY_TYPE_TIMESTAMP);

    platform::vk_query pipeline_statistics_query;
    if (pipeline_stats_supported)
    {
        pipeline_statistics_query = *platform::vk_create_pipeline_stat_query_pool(
            m_renderer.get_context().device,
            kQueryPoolCount,
            VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
                | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
                | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
                | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
                | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT);
    }
#endif

    rhi::buffer draw_count_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = sizeof(u32[shader_constants::kMatClassCount * 3]),
         .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw});

    rhi::buffer indexed_count_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = sizeof(u32[2]),
         .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw});

    rhi::buffer mesh_visibility_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = (scene_counters.instances + 31) / 8,
         .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw});

    rhi::buffer meshlets_visibility_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = (scene_counters.meshlets + 31) / 8,
         .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw});

    rhi::buffer indexed_indices_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = 96_MB,
         .usage_flags = rhi::buffer_usage::index | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw});

    rhi::buffer indexed_draw_indirect_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = 16_MB,
         .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw});

    rhi::buffer meshlets_draw_indirect_buffer = *m_rhi.create_buffer(
        *context,
        {.size        = 16_MB,
         .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw});

    cpp::heap_array<mapped_buffer> world_data_buffers(*m_rhi.query_swapchain_images_count(*swapchain));
    cpp::heap_array<mapped_buffer> frame_cull_data_buffers(*m_rhi.query_swapchain_images_count(*swapchain));
    cpp::heap_array<mapped_buffer> shadow_cascades_data_buffers(*m_rhi.query_swapchain_images_count(*swapchain));

    for (u32 i = 0; i < *m_rhi.query_swapchain_images_count(*swapchain); i++)
    {
        world_data_buffers[i] = mapped_buffer::create(m_rhi,
                                                      *context,
                                                      rhi::create_buffer_info {
                                                          .size        = sizeof(shader_types::FrameWorldData),
                                                          .usage_flags = rhi::buffer_usage::shader_rw,
                                                      });

        shadow_cascades_data_buffers[i] = mapped_buffer::create(m_rhi,
                                                                *context,
                                                                rhi::create_buffer_info {
                                                                    .size = sizeof(shader_types::ShadowCascadesData),
                                                                    .usage_flags = rhi::buffer_usage::shader_rw,
                                                                });

        frame_cull_data_buffers[i] = mapped_buffer::create(m_rhi,
                                                           *context,
                                                           rhi::create_buffer_info {
                                                               .size        = sizeof(shader_types::FrameCullData),
                                                               .usage_flags = rhi::buffer_usage::shader_rw,
                                                           });
    }

    gpu_profile_data profile_data;
    render_settings client_render_settings;
#if !NO_PERF_QUERY
    cpp::heap_array<pipeline_statistics_data> pipeline_statistics_data;
#endif

    glm::mat4 camera_proj;
    glm::mat4 camera_view;
    glm::mat4 camera_proj_view;
    glm::mat4 debug_camera_view;

    u32 draw_materials_mask = 0xFFFF;
    debug_mode draw_debug_mode {debug_mode::shaded};

    f32 camera_exposure        = 10.0F;
    f32 envmap_compensation_ev = 0.0f;
    f32 envmap_intensity       = 1250.0f;

    bool freeze_cull_data = false;
    // bool enable_vsync             = m_renderer.get_vsync();
    bool enable_fullscreen        = m_window.get_fullscreen();
    bool enable_meshlets_pipeline = mesh_shading_supported;

    auto get_time = []<typename T = f64>()
    {
        return static_cast<T>(SDL_GetPerformanceCounter()) / static_cast<T>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    camera_controller controller(m_events_queue, camera);

    app::debug::frustum_renderer frustum_renderer(pipelines[pso_id::frustum_debug]);

#if !NO_EDITOR
    editor::hierarchy_window_context hierarchy_window_context;
    editor::info_widget_context info_widget_context {
        .m_camera        = controller,
        .m_gpu_profile   = profile_data,
        .m_geometry_pool = geometry_pool,
    };
#endif

    const auto offset_table = make_offset_table(scene_counters.mat_offset_table);
    auto fill_indexed       = [&](rhi::command_buffer cmd,
                            const u32 material_class,
                            const bool enable_occlusion_cull,
                            const bool for_shadow_pass     = false,
                            const u32 shadow_cascade_index = 0)
    {
        if (enable_meshlets_pipeline)
        {
            return;
        }

        // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "build index buffer late"));

        app::zero_buffer(m_rhi, cmd, indexed_count_buffer, sizeof(u32));
#if defined(__APPLE__)
        app::zero_buffer(m_rhi, cmd, indexed_draw_indirect_buffer);
#endif

        descriptor_bindings bindings;
        bindings.bind_at(geometry_pool.meshlets, shader_bindings::fill::kMeshletBinding)
            .bind_at(geometry_pool.meshlets_payload, shader_bindings::fill::kMeshletDataBinding)
            .bind_at(geometry_pool.primitives, shader_bindings::fill::kPrimitiveBinding)
            .bind_at(geometry_pool.instances, shader_bindings::fill::kInstanceBinding)
            .bind_at(meshlets_draw_indirect_buffer, shader_bindings::fill::kDrawBinding)
            .bind_at(for_shadow_pass ? shadow_cascades_data_buffers[*m_rhi.query_current_frame_index(*swapchain)].buffer
                                     : frame_cull_data_buffers[*m_rhi.query_current_frame_index(*swapchain)].buffer,
                     shader_bindings::fill::kCullBinding)
            .bind_at(indexed_indices_buffer, shader_bindings::fill::kOutIndicesBinding)
            .bind_at(indexed_draw_indirect_buffer, shader_bindings::fill::kOutCommandsBinding)
            .bind_at(indexed_count_buffer, shader_bindings::fill::kOutCountBinding);

        if (!for_shadow_pass)
        {
            bindings.bind_at(meshlets_visibility_buffer, shader_bindings::fill::kVisibilityBinding);
            if (enable_occlusion_cull)
            {
                bindings.bind_at(rhi::binding(depth_pyramid.image, depth_pyramid.sampler),
                                 shader_bindings::fill::kHiZBinding);
            }
        }

        pso_id id;
        switch (material_class)
        {
        case shader_constants::kMatClassMasked :
        case shader_constants::kMatClassTranslucent :
            id = for_shadow_pass
                   ? pso_id::shadow_fill_ds
                   : (enable_occlusion_cull ? pso_id::indexed_fill_ds_late_pipeline : pso_id::indexed_fill_ds_pipeline);
            break;
        default :
        case shader_constants::kMatClassOpaque :
            id = for_shadow_pass
                   ? pso_id::shadow_fill_ss
                   : (enable_occlusion_cull ? pso_id::indexed_fill_late_pipeline : pso_id::indexed_fill_pipeline);
            break;
        }

        const rhi::pipeline& fill_pass = pipelines[id];

        m_rhi.cmd_bind_pso(cmd, fill_pass);
        m_rhi.cmd_push_constants(
            cmd, fill_pass, &offset_table[material_class], sizeof(offset_table[material_class]), 0);

        if (for_shadow_pass)
            m_rhi.cmd_push_constants(cmd,
                                     fill_pass,
                                     &shadow_cascade_index,
                                     sizeof(shadow_cascade_index),
                                     sizeof(offset_table[material_class]));

        m_rhi.cmd_push_bindings(cmd, fill_pass, bindings.get());
        m_rhi.cmd_dispatch_indirect(cmd, draw_count_buffer, material_class * 3 * sizeof(u32));

        constexpr rhi::global_barrier barrier {
            .before =
                {
                         .stages = rhi::barrier_stage::compute_shader,
                         .access = rhi::barrier_access::storage_write,
                         },
            .after =
                {
                         .stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect
                            | rhi::barrier_stage::all_graphics,
                         .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                            | rhi::barrier_access::index_read,
                         },
        };

        const rhi::barrier_batch barriers {
            .globals = {&barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, barriers);
    };

    auto draw_scene = [&](rhi::command_buffer cmd, const rhi::pipeline& pipeline, const u32 material_class)
    {
        ZoneScopedN("app.instance.run.draw_scene");
        // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "draw scene"));

        constexpr rhi::barrier_stage_bits kAttachmentStages = rhi::barrier_stage::color_attachment
                                                            | rhi::barrier_stage::early_depth_stencil
                                                            | rhi::barrier_stage::late_depth_stencil;
        constexpr rhi::barrier_access_bits kAttachmentAccess =
            rhi::barrier_access::color_attachment_read | rhi::barrier_access::color_attachment_write
            | rhi::barrier_access::depth_stencil_read | rhi::barrier_access::depth_stencil_write;

        constexpr rhi::global_barrier barrier {
            .before = {.stages = kAttachmentStages, .access = kAttachmentAccess},
            .after  = {.stages = kAttachmentStages, .access = kAttachmentAccess},
        };

        const rhi::barrier_batch barriers {
            .globals = {&barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, barriers);

        const rhi::attachment_state_info color_attachments[] = {
            {.attachment = vis_buffer,
             .load_op    = rhi::resource_load_op::load,
             .store_op   = rhi::resource_store_op::store},
        };
        m_rhi.cmd_set_draw_state(cmd,
                                 color_attachments,
                                 {.attachment = depth_image,
                                  .load_op    = rhi::resource_load_op::load,
                                  .store_op   = rhi::resource_store_op::store},
                                 {0, 0, m_window.get_size_in_px().x, m_window.get_size_in_px().y});

        if (material_class != shader_constants::kMatClassOpaque)
        {
            m_rhi.cmd_push_bindless_set(cmd, pipeline, *bindless_textures_desc_set, 1);
        }

        descriptor_bindings bindings;
        bindings.bind_at(geometry_pool.vertex, shader_bindings::draw::kVertexBinding);
        bindings.bind_at(geometry_pool.materials, shader_bindings::draw::kMaterialBinding);
        bindings.bind_at(rhi::binding(*bindless_textures_sampler), shader_bindings::draw::kTextureBinding);
        bindings.bind_at(geometry_pool.meshlets, shader_bindings::draw::kMeshletBinding);
        bindings.bind_at(geometry_pool.meshlets_payload, shader_bindings::draw::kMeshletDataBinding);
        bindings.bind_at(geometry_pool.primitives, shader_bindings::draw::kPrimitiveBinding);
        bindings.bind_at(geometry_pool.instances, shader_bindings::draw::kInstanceBinding);

        if (enable_meshlets_pipeline)
        {
            bindings.bind_at(meshlets_draw_indirect_buffer, shader_bindings::draw::kDrawBinding);
            bindings.bind_at(frame_cull_data_buffers[*m_rhi.query_current_frame_index(*swapchain)].buffer,
                             shader_bindings::draw::kCullBinding);
            bindings.bind_at(meshlets_visibility_buffer, shader_bindings::draw::kVisibilityBinding);
            bindings.bind_at(rhi::binding(depth_pyramid.image, depth_pyramid.sampler),
                             shader_bindings::draw::kHiZBinding);

            m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
            m_rhi.cmd_draw_mesh_indirect(cmd, draw_count_buffer, material_class * 3 * sizeof(u32), 1, 0);
        }
        else
        {
            bindings.bind_at(indexed_draw_indirect_buffer, shader_bindings::draw::kDrawBinding);

            m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
            m_rhi.cmd_bind_index(cmd, indexed_indices_buffer);

            // pipeline_statistics_query.begin_next(cmd);
#if defined(__APPLE__)
            m_rhi.cmd_draw_indexed_indirect(cmd,
                                            indexed_draw_indirect_buffer,
                                            0,
                                            scene_info.mat_offset_table[material_class],
                                            sizeof(shader_types::DrawIndexedIndirect));
#else
            m_rhi.cmd_draw_indexed_indirect_count(cmd,
                                                  indexed_draw_indirect_buffer,
                                                  0,
                                                  indexed_count_buffer,
                                                  sizeof(u32),
                                                  scene_counters.mat_offset_table[material_class],
                                                  sizeof(shader_types::DrawIndexedIndirect));
#endif
            // pipeline_statistics_query.end_and_advance(cmd);
        }

        m_rhi.cmd_clear_draw_state(cmd);
    };

    auto draw_shadow =
        [&](rhi::command_buffer cmd, const rhi::pipeline& pipeline, const u32 material_class, const u32 cascade_index)
    {
        ZoneScopedN("app.instance.run.draw_shadow");
        // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "draw shadow"));

        constexpr rhi::barrier_stage_bits kAttachmentStages =
            rhi::barrier_stage::early_depth_stencil | rhi::barrier_stage::late_depth_stencil;
        constexpr rhi::barrier_access_bits kAttachmentAccess =
            rhi::barrier_access::depth_stencil_read | rhi::barrier_access::depth_stencil_write;

        constexpr rhi::global_barrier barrier {
            .before = {.stages = kAttachmentStages, .access = kAttachmentAccess},
            .after  = {.stages = kAttachmentStages, .access = kAttachmentAccess},
        };

        const rhi::barrier_batch barriers {
            .globals = {&barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, barriers);

        m_rhi.cmd_set_draw_state(cmd,
                                 {},
                                 {.attachment = csm.cascade_views[cascade_index],
                                  .load_op    = rhi::resource_load_op::load,
                                  .store_op   = rhi::resource_store_op::store},
                                 {0, 0, csm.resolution, csm.resolution});

        descriptor_bindings bindings;
        bindings.bind_at(geometry_pool.vertex, shader_bindings::shadow_draw::kVertexBinding);
        bindings.bind_at(geometry_pool.materials, shader_bindings::shadow_draw::kMaterialBinding);
        bindings.bind_at(rhi::binding(*shadow_alpha_sampler), shader_bindings::shadow_draw::kTextureBinding);
        bindings.bind_at(geometry_pool.meshlets, shader_bindings::shadow_draw::kMeshletBinding);
        bindings.bind_at(geometry_pool.meshlets_payload, shader_bindings::shadow_draw::kMeshletDataBinding);
        bindings.bind_at(geometry_pool.primitives, shader_bindings::shadow_draw::kPrimitiveBinding);
        bindings.bind_at(geometry_pool.instances, shader_bindings::shadow_draw::kInstanceBinding);

        if (material_class != shader_constants::kMatClassOpaque)
        {
            m_rhi.cmd_push_bindless_set(cmd, pipeline, *bindless_textures_desc_set, 1);
        }

        if (enable_meshlets_pipeline)
        {
            bindings.bind_at(meshlets_draw_indirect_buffer, shader_bindings::shadow_draw::kDrawBinding);
            bindings.bind_at(shadow_cascades_data_buffers[*m_rhi.query_current_frame_index(*swapchain)].buffer,
                             shader_bindings::shadow_draw::kCullBinding);

            m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
            m_rhi.cmd_draw_mesh_indirect(cmd, draw_count_buffer, material_class * 3 * sizeof(u32), 1, 0);
        }
        else
        {
            bindings.bind_at(indexed_draw_indirect_buffer, shader_bindings::shadow_draw::kDrawBinding);

            m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
            m_rhi.cmd_bind_index(cmd, indexed_indices_buffer);

#if defined(__APPLE__)
            m_rhi.cmd_draw_indexed_indirect(cmd,
                                            indexed_draw_indirect_buffer,
                                            0,
                                            scene_info.mat_offset_table[material_class],
                                            sizeof(shader_types::DrawIndexedIndirect));
#else
            m_rhi.cmd_draw_indexed_indirect_count(cmd,
                                                  indexed_draw_indirect_buffer,
                                                  0,
                                                  indexed_count_buffer,
                                                  sizeof(u32),
                                                  scene_counters.mat_offset_table[material_class],
                                                  sizeof(shader_types::DrawIndexedIndirect));
#endif
        }

        m_rhi.cmd_clear_draw_state(cmd);
    };

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
            // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "load envmap"));
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

    rhi::ds_clear_value ds_clear    = {.depth = 0.0F, .stencil = 0};
    rhi::color_clear_value vb_clear = {.u4 = uvec4(~0U)};

    auto render_loop = [&]()
    {
        ZoneScopedN("app.instance.run.render_loop");
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update(static_cast<f32>(dt));

        // if (m_renderer.get_vsync() != enable_vsync)
        // {
        //     m_renderer.set_vsync(enable_vsync);
        //     return;
        // }

        const auto frame_image = m_rhi.acquire_next_swapchain_image(*context, *swapchain);
        if (!frame_image)
        {
            return;
        }

        tracker.next_frame();
        tracker.submit(
            [&](const rhi::command_buffer buffer)
            {
                ZoneScopedN("main.m_renderer.submit");

                // const auto viewport = m_renderer.get_viewport();
                // const auto scissor  = m_renderer.get_scissor();

                m_rhi.cmd_begin_recording(buffer);
            // TRACY_ONLY(TracyVkCollect(m_renderer.get_frame_tracy_context(), buffer));

#if !NO_PERF_QUERY
                timestamp_query_pool.reset(buffer, 0, kQueryPoolCount);
                pipeline_statistics_query.reset(buffer, 0, kQueryPoolCount);

                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool.handle, 0);
#endif

                auto& frame_cull_data_buffer = frame_cull_data_buffers[*m_rhi.query_current_frame_index(*swapchain)];
                if (!freeze_cull_data)
                {
                    auto projection = client_render_settings.render_distance > 0
                                        ? camera_data.get_projection_matrix(client_render_settings.render_distance)
                                        : camera_data.get_projection_matrix();

                    auto view = camera_component::get_view_matrix(camera_transform.position, camera_transform.rotation);
                    debug_camera_view = view;

                    shader_types::FrameCullData fcd {.pyramid_size  = depth_pyramid.base_size,
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
                camera_proj_view = camera_proj * camera_view;

                auto& sun_transform = sun.get_component<transform_component>();
                auto& sun_data      = sun.get_component<directional_light_component>();

                auto sun_direction = glm::normalize(glm::mat3_cast(sun_transform.rotation) * vec3(0, 0, 1));

                auto& world_data_buffer = world_data_buffers[*m_rhi.query_current_frame_index(*swapchain)];
                (*static_cast<shader_types::FrameWorldData*>(world_data_buffer.mapped)) = shader_types::FrameWorldData {
                    .sun_color         = {sun_data.rgb_color, sun_data.intensity},
                    .camera_pos        = camera_transform.position,
                    .debug_mode        = static_cast<u32>(draw_debug_mode),
                    .sun_direction     = sun_direction,
                    .camera_exposure   = camera_exposure,
                    .envmap_scale      = envmap_intensity * glm::exp2(envmap_compensation_ev),
                    .render_resolution = m_window.get_size_in_px(),
                };

                auto& shadow_cascades_data_buffer =
                    shadow_cascades_data_buffers[*m_rhi.query_current_frame_index(*swapchain)];
                const auto light_cascades_vps = update_csm_buffers(csm,
                                                                   shadow_cascades_data_buffer,
                                                                   sun_direction,
                                                                   camera_data,
                                                                   freeze_cull_data ? debug_camera_view : camera_view,
                                                                   camera_transform.position,
                                                                   client_render_settings);

                app::zero_buffer(m_rhi, buffer, indexed_count_buffer);

                m_rhi.cmd_clear_depth_attachment(buffer, csm.shadow_map, {.depth = 0.0F, .stencil = 0});
                for (u32 c = 0; c < shader_constants::kMaxShadowCascades; ++c)
                {
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull cascade meshes"));
                    reset_draw_count_buffer(m_rhi, buffer, draw_count_buffer);

                    descriptor_bindings cull_pass_bindings;
                    cull_pass_bindings
                        .bind_at(geometry_pool.primitives, shader_bindings::shadow_cull::kPrimitiveBinding)
                        .bind_at(geometry_pool.instances, shader_bindings::shadow_cull::kInstanceBinding)
                        .bind_at(geometry_pool.materials, shader_bindings::shadow_cull::kMaterialBinding)
                        .bind_at(draw_count_buffer, shader_bindings::shadow_cull::kDrawCountBinding)
                        .bind_at(frame_cull_data_buffer.buffer, shader_bindings::shadow_cull::kFrameCullBinding)
                        .bind_at(shadow_cascades_data_buffer.buffer, shader_bindings::shadow_cull::kCascadeCullBinding)
                        .bind_at(meshlets_draw_indirect_buffer, shader_bindings::shadow_cull::kOutDrawBinding);

                    const rhi::pipeline cull_pass = pipelines[pso_id::shadow_cull];
                    m_rhi.cmd_bind_pso(buffer, cull_pass);
                    m_rhi.cmd_push_bindings(buffer, cull_pass, cull_pass_bindings.get());

                    m_rhi.cmd_push_constants(buffer, cull_pass, &c, sizeof(c), 0);
                    m_rhi.cmd_push_constants(buffer, cull_pass, &offset_table, sizeof(offset_table), sizeof(c));

                    m_rhi.cmd_dispatch(buffer, cull_pass, {static_cast<u32>(scene_counters.instances), 1, 1});

                    rhi::global_barrier barrier {
                        .before = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::storage_write                                    },
                        .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect
                                           | rhi::barrier_stage::all_graphics,
                                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read},
                    };

                    m_rhi.cmd_barriers(buffer,
                                       {
                                           .globals = std::span {&barrier, 1}
                    });

                    for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                    {
                        if (!(draw_materials_mask & (1 << i)))
                        {
                            continue;
                        }

                        fill_indexed(buffer, i, false, true, c);
                        const auto& render_pipeline = get_shadow_pipeline(pipelines, i, enable_meshlets_pipeline);
                        m_rhi.cmd_bind_pso(buffer, render_pipeline);

                        shader_types::ShadowDrawPushConstants pc(light_cascades_vps[c], offset_table[i], c);
                        m_rhi.cmd_push_constants(buffer, render_pipeline, &pc, sizeof(pc), 0);
                        m_rhi.cmd_set_cull_mode(buffer,
                                                i == shader_constants::kMatClassOpaque ? rhi::cull_mode::back
                                                                                       : rhi::cull_mode::none);
                        m_rhi.cmd_set_depth_bias(buffer,
                                                 client_render_settings.shadow_depth_bias_constant,
                                                 client_render_settings.shadow_depth_bias_slope,
                                                 0.0F);

                        draw_shadow(buffer, render_pipeline, i, c);
                    }
                }

                {
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull last frame
                    // occluders"));

                    reset_draw_count_buffer(m_rhi, buffer, draw_count_buffer);
                    const rhi::binding cull_pass_bindings[] = {
                        geometry_pool.primitives,
                        geometry_pool.instances,
                        geometry_pool.materials,
                        draw_count_buffer,
                        mesh_visibility_buffer,
                        frame_cull_data_buffer.buffer,
                        meshlets_draw_indirect_buffer,
                    };

                    const rhi::pipeline& cull_pass = pipelines[pso_id::task_cull_pipeline];

                    m_rhi.cmd_bind_pso(buffer, cull_pass);
                    m_rhi.cmd_push_constants(buffer, cull_pass, &offset_table, sizeof(offset_table), 0);
                    m_rhi.cmd_push_bindings(buffer, cull_pass, std::span {cull_pass_bindings});

                    m_rhi.cmd_dispatch(buffer, cull_pass, {static_cast<u32>(scene_counters.instances), 1, 1});

                    rhi::global_barrier barrier {
                        .before = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::storage_write},
                        .after  = {
                                   .stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect
                                    | rhi::barrier_stage::all_graphics,
                                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read,
                                   }
                    };

                    rhi::barrier_batch barriers {
                        .globals = {&barrier, 1}
                    };
                    m_rhi.cmd_barriers(buffer, barriers);
                }
                app_cmd_transition_image(m_rhi, buffer, render_target, rhi::image_layout::common);

                app_cmd_transition_image(m_rhi, buffer, *frame_image, rhi::image_layout::common);

                app_cmd_transition_image(m_rhi, buffer, vis_buffer, rhi::image_layout::common);

                app_cmd_transition_image(
                    m_rhi, buffer, depth_image, rhi::image_layout::common, rhi::image_aspect::depth);

                m_rhi.cmd_clear_depth_attachment(buffer, depth_image, ds_clear);

                m_rhi.cmd_clear_color_attachment(buffer, vis_buffer, vb_clear);

                app::zero_buffer(m_rhi, buffer, indexed_count_buffer);
                for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                {
                    if (!(draw_materials_mask & (1 << i)))
                    {
                        continue;
                    }

                    fill_indexed(buffer, i, false);
                    const auto& render_pipeline = get_render_pipeline(pipelines, i, false, enable_meshlets_pipeline);

                    m_rhi.cmd_bind_pso(buffer, render_pipeline);
                    const auto push_constants = shader_types::DrawPushConstants {
                        freeze_cull_data ? camera_proj * debug_camera_view : camera_proj_view, offset_table[i]};
                    m_rhi.cmd_push_constants(buffer, render_pipeline, &push_constants, sizeof(push_constants), 0);

                    m_rhi.cmd_set_cull_mode(
                        buffer, i == shader_constants::kMatClassOpaque ? rhi::cull_mode::back : rhi::cull_mode::none);

                    draw_scene(buffer, render_pipeline, i);
                }

                // Reduce the depth buffer pyramid
                {
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "depth reduce"));

                    constexpr rhi::global_barrier barrier {
                        .before = {.stages =
                                       rhi::barrier_stage::early_depth_stencil | rhi::barrier_stage::late_depth_stencil,
                                   .access = rhi::barrier_access::depth_stencil_write},
                        .after  = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::sampled_read       },
                    };

                    const rhi::barrier_batch barriers {
                        .globals = {&barrier, 1}
                    };

                    m_rhi.cmd_barriers(buffer, barriers);

                    app_cmd_transition_image(m_rhi, buffer, depth_pyramid.image, rhi::image_layout::common);

                    const auto& depth_reduce_pipeline = pipelines[pso_id::depth_reduce_pipeline];
                    m_rhi.cmd_bind_pso(buffer, depth_reduce_pipeline);

                    for (i32 i = 0; i < depth_pyramid.pyramid_count; ++i)
                    {
                        const rhi::binding cull_pass_bindings[] = {
                            rhi::binding(i == 0 ? rhi::attachment(depth_image)
                                                : rhi::attachment(depth_pyramid.views[i - 1]),
                                         depth_pyramid.sampler),
                            rhi::binding(depth_pyramid.views[i], depth_pyramid.sampler),
                        };

                        m_rhi.cmd_push_bindings(buffer, depth_reduce_pipeline, std::span {cull_pass_bindings});

                        const ivec2 out_size = glm::max(depth_pyramid.base_size >> i, ivec2(1));
                        const vec2 push_constants(out_size);
                        m_rhi.cmd_push_constants(
                            buffer, depth_reduce_pipeline, &push_constants, sizeof(push_constants), 0);
                        m_rhi.cmd_dispatch(buffer, depth_reduce_pipeline, {out_size.x, out_size.y, 1});

                        constexpr rhi::global_barrier dp_barrier {
                            .before = {.stages = rhi::barrier_stage::compute_shader,
                                       .access = rhi::barrier_access::storage_write                                   },
                            .after  = {.stages = rhi::barrier_stage::compute_shader,
                                       .access = rhi::barrier_access::sampled_read | rhi::barrier_access::storage_read},
                        };

                        const rhi::barrier_batch dp_barriers {
                            .globals = {&dp_barrier, 1}
                        };

                        m_rhi.cmd_barriers(buffer, dp_barriers);
                    }
                }

                // NOTE: only executed if freeze_cull_data == true
                if (freeze_cull_data)
                {
                    m_rhi.cmd_clear_depth_attachment(buffer, depth_image, ds_clear);

                    m_rhi.cmd_clear_color_attachment(buffer, vis_buffer, vb_clear);

                    for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                    {
                        if (!(draw_materials_mask & (1 << i)))
                        {
                            continue;
                        }

                        fill_indexed(buffer, i, false);
                        const auto& render_pipeline =
                            get_render_pipeline(pipelines, i, false, enable_meshlets_pipeline);

                        m_rhi.cmd_bind_pso(buffer, render_pipeline);
                        const auto push_constants = shader_types::DrawPushConstants(camera_proj_view, offset_table[i]);
                        m_rhi.cmd_push_constants(buffer, render_pipeline, &push_constants, sizeof(push_constants), 0);
                        m_rhi.cmd_set_cull_mode(buffer,
                                                i == shader_constants::kMatClassOpaque ? rhi::cull_mode::back
                                                                                       : rhi::cull_mode::none);

                        draw_scene(buffer, render_pipeline, i);
                    }
                }

                {
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull new objects"));

                    reset_draw_count_buffer(m_rhi, buffer, draw_count_buffer);
                    const rhi::binding cull_pass_bindings[] = {
                        geometry_pool.primitives,
                        geometry_pool.instances,
                        geometry_pool.materials,
                        draw_count_buffer,
                        mesh_visibility_buffer,
                        frame_cull_data_buffer.buffer,
                        meshlets_draw_indirect_buffer,
                        rhi::binding(depth_pyramid.image, depth_pyramid.sampler)};

                    const rhi::pipeline& cull_pass = pipelines[pso_id::task_occlusion_cull_pipeline];

                    m_rhi.cmd_bind_pso(buffer, cull_pass);
                    m_rhi.cmd_push_constants(buffer, cull_pass, &offset_table, sizeof(offset_table), 0);
                    m_rhi.cmd_push_bindings(buffer, cull_pass, std::span {cull_pass_bindings});

                    m_rhi.cmd_dispatch(buffer, cull_pass, {static_cast<u32>(scene_counters.instances), 1, 1});

                    constexpr rhi::global_barrier barrier {
                        .before = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::storage_write                                    },
                        .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect
                                           | rhi::barrier_stage::all_graphics,
                                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read},
                    };

                    const rhi::barrier_batch barriers {
                        .globals = {&barrier, 1}
                    };

                    m_rhi.cmd_barriers(buffer, barriers);
                }

                for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                {
                    if (!((draw_materials_mask)
                          // if (!((draw_materials_mask & ~static_cast<u64>(1 <<
                          // shader_constants::kMatClassTranslucent))
                          & (1 << i)))
                    {
                        continue;
                    }

                    fill_indexed(buffer, i, true);
                    const auto& render_pipeline = get_render_pipeline(pipelines, i, true, enable_meshlets_pipeline);

                    m_rhi.cmd_bind_pso(buffer, render_pipeline);
                    const auto push_constants = shader_types::DrawPushConstants(camera_proj_view, offset_table[i]);
                    m_rhi.cmd_push_constants(buffer, render_pipeline, &push_constants, sizeof(push_constants), 0);

                    m_rhi.cmd_set_cull_mode(
                        buffer, i == shader_constants::kMatClassOpaque ? rhi::cull_mode::back : rhi::cull_mode::none);
                    draw_scene(buffer, render_pipeline, i);
                }

                {
                    ZoneScopedN("Resolve pass");
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "vb resolve"));

                    {
                        constexpr rhi::global_barrier barrier {
                            .before = {.stages = rhi::barrier_stage::color_attachment
                                               | rhi::barrier_stage::late_depth_stencil,
                                       .access = rhi::barrier_access::color_attachment_write
                                               | rhi::barrier_access::depth_stencil_write                             },
                            .after  = {.stages = rhi::barrier_stage::compute_shader,
                                       .access = rhi::barrier_access::storage_read | rhi::barrier_access::sampled_read},
                        };

                        const rhi::barrier_batch barriers {
                            .globals = {&barrier, 1}
                        };

                        m_rhi.cmd_barriers(buffer, barriers);
                    }

                    const rhi::binding resolve_pass_bindings[] = {
                        rhi::binding(rhi::attachment(render_target)),
                        rhi::binding(rhi::attachment(vis_buffer)),
                        indexed_indices_buffer,
                        geometry_pool.vertex,
                        geometry_pool.meshlets,
                        geometry_pool.meshlets_payload,
                        geometry_pool.primitives,
                        geometry_pool.instances,
                        geometry_pool.materials,
                        world_data_buffer.buffer,
                        shadow_cascades_data_buffer.buffer,
                        rhi::binding(*bindless_textures_sampler),
                        rhi::binding(depth_image, *depth_texture_sampler),
                        envmap.get_lut_descriptor_info(),
                        envmap.get_cube_descriptor_info(),
                        envmap.get_conv_descriptor_info(),
                        envmap.get_pref_descriptor_info(),
                        csm.get_descriptor_info(),
                    };

                    const rhi::pipeline& resolve_pass =
                        pipelines[enable_meshlets_pipeline ? pso_id::mesh_resolve_pipeline
                                                           : pso_id::vert_resolve_pipeline];

                    m_rhi.cmd_bind_pso(buffer, resolve_pass);
                    m_rhi.cmd_push_bindings(buffer, resolve_pass, std::span {resolve_pass_bindings});
                    m_rhi.cmd_push_bindless_set(buffer, resolve_pass, *bindless_textures_desc_set, 1);

                    const auto push_constants = shader_types::ResolvePassPushConstants(camera_view, camera_proj);
                    m_rhi.cmd_push_constants(buffer, resolve_pass, &push_constants, sizeof(push_constants), 0);

                    m_rhi.cmd_dispatch(
                        buffer, resolve_pass, {m_window.get_size_in_px().x, m_window.get_size_in_px().y, 1});

                    {
                        constexpr rhi::global_barrier barrier {
                            .before = {.stages = rhi::barrier_stage::compute_shader,
                                       .access = rhi::barrier_access::storage_write},
                            .after  = {.stages = rhi::barrier_stage::compute_shader,
                                       .access = rhi::barrier_access::sampled_read },
                        };

                        const rhi::barrier_batch barriers {
                            .globals = {&barrier, 1}
                        };

                        m_rhi.cmd_barriers(buffer, barriers);
                    }
                }

                {
                    ZoneScopedN("FXAA pass");
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "vb resolve"));

                    const rhi::binding fxaa_pass_bindings[] = {
                        rhi::binding(*frame_image),
                        rhi::binding(render_target, *color_sampler),
                        rhi::binding(depth_image, *depth_texture_sampler),
                    };

                    const rhi::pipeline& fxaa_pass = pipelines[pso_id::fxaa_pipeline];

                    m_rhi.cmd_bind_pso(buffer, fxaa_pass);
                    m_rhi.cmd_push_bindings(buffer, fxaa_pass, std::span {fxaa_pass_bindings});

                    m_rhi.cmd_push_constants(
                        buffer, fxaa_pass, &camera_data.near_plane, sizeof(camera_data.near_plane), 0);
                    const auto viewport_size = m_window.get_size_in_px();
                    m_rhi.cmd_push_constants(
                        buffer, fxaa_pass, &viewport_size, sizeof(viewport_size), sizeof(camera_data.near_plane));

                    m_rhi.cmd_dispatch(
                        buffer, fxaa_pass, {m_window.get_size_in_px().x, m_window.get_size_in_px().y, 1});

                    constexpr rhi::global_barrier barrier {
                        .before = {.stages = rhi::barrier_stage::compute_shader,
                                   .access = rhi::barrier_access::storage_write         },
                        .after  = {.stages = rhi::barrier_stage::color_attachment,
                                   .access = rhi::barrier_access::color_attachment_read
                                           | rhi::barrier_access::color_attachment_write},
                    };

                    const rhi::barrier_batch barriers {
                        .globals = {&barrier, 1}
                    };

                    m_rhi.cmd_barriers(buffer, barriers);
                }

                if (freeze_cull_data)
                {
                    ZoneScopedN("Frustum debug render pass");
                    // TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "frustum debug"));

                    const rhi::attachment_state_info color_attachments[] = {
                        {.attachment = *frame_image,
                         .load_op    = rhi::resource_load_op::load,
                         .store_op   = rhi::resource_store_op::store},
                    };

                    m_rhi.cmd_set_draw_state(buffer,
                                             {color_attachments, COUNT_OF(color_attachments)},
                                             {.attachment = depth_image,
                                              .load_op    = rhi::resource_load_op::load,
                                              .store_op   = rhi::resource_store_op::store},
                                             {0, 0, m_window.get_size_in_px().x, m_window.get_size_in_px().y});

                    frustum_renderer.draw(m_rhi, buffer, camera_proj_view, frame_cull_data_buffer.buffer);
                    m_rhi.cmd_clear_draw_state(buffer);
                }

#if !NO_EDITOR
                {
                    ZoneScopedN("main.draw.editor");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "editor"));

                    editor.begin_frame();

                    hierarchy_window_context.draw(client_scene);

                    if (ImGui::Begin("Render controls"))
                    {
                        info_widget_context.draw();
                        info_widget_context.draw("Pipeline stats", pipeline_statistics_data);
                        ImGui::SeparatorText("render controls");

                        const char* names[] = {
                            "LODs",
                            "Frustum cull",
                            "Occlusion cull",
                            "Meshlets cone cull",
                            "Meshlets frustum cull",
                            "Meshlets occlusion cull",
                            "Small meshlets cull",
                        };
                        ImGuiWidgets::Bits(client_render_settings.flags, names, COUNT_OF(names));

                        const char* material_classes[] = {
                            "Opaque",
                            "Masked",
                            "Translucent",
                        };
                        ImGuiWidgets::Bits(draw_materials_mask, material_classes, COUNT_OF(material_classes));

                        ImGuiWidgets::EnumDrag("Debug mode", draw_debug_mode);

                        codegen::draw(client_render_settings);

                        ImGui::BeginDisabled(!mesh_shading_supported);
                        ImGui::Checkbox("Enable meshlets path", &enable_meshlets_pipeline);
                        ImGui::EndDisabled();

                        ImGui::Checkbox("Enable vsync", &enable_vsync);
                        if (ImGui::Checkbox("Enable fullscreen", &enable_fullscreen))
                        {
                            m_window.set_fullscreen(enable_fullscreen);
                        }

                        {
                            ImGuiEx::ScopedColor btn(ImGuiCol_Button,
                                                     freeze_cull_data ? IM_COL32(180, 60, 60, 255)
                                                                      : IM_COL32(60, 60, 65, 255));
                            ImGuiEx::ScopedColor btn_hover(ImGuiCol_ButtonHovered,
                                                           freeze_cull_data ? IM_COL32(210, 85, 85, 255)
                                                                            : IM_COL32(80, 80, 85, 255));
                            ImGuiEx::ScopedColor btn_active(ImGuiCol_ButtonActive,
                                                            freeze_cull_data ? IM_COL32(230, 110, 110, 255)
                                                                             : IM_COL32(100, 100, 105, 255));

                            if (ImGui::Button(freeze_cull_data ? "Unfreeze cull data" : "Freeze cull data"))
                            {
                                freeze_cull_data = !freeze_cull_data;
                            }

                            if (freeze_cull_data)
                            {
                                static ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;

                                auto tmp = glm::inverse(debug_camera_view);
                                ImGuiWidgets::Gizmo(camera_view, camera_proj, tmp, op);
                                debug_camera_view = glm::inverse(tmp);
                            }
                        }

                        if (ImGui::CollapsingHeader("Environment map", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::InputText("Env map", env_map.data(), fs::path_string::capacity());
                            if (ImGui::Button("Load"))
                            {
                                envmap.load(env_map, pipelines, m_renderer, geometry_pool.transfer);
                            }

                            constexpr ImGuiTableFlags flags =
                                ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;

                            ImGui::DragFloat("Env intensity", &envmap_intensity);
                            ImGui::DragFloat("Env compensation", &envmap_compensation_ev);

                            if (!env_map.empty() && ImGui::BeginTable("Environment maps", 3, flags))
                            {
                                ImGui::TableSetupColumn("Environment");
                                ImGui::TableSetupColumn("Convolution");
                                ImGui::TableSetupColumn("Prefiltered");
                                ImGui::TableHeadersRow();

                                ImGui::TableNextRow();

                                const auto image_size = []
                                {
                                    const f32 width = ImGui::GetContentRegionAvail().x;
                                    return ImVec2 {width, width};
                                };

                                ImGui::TableSetColumnIndex(0);
                                auto env_params = ImGuiWidgets::ImageControls("Environment", 12.0F, 5.0F);

                                editor.image_array(envmap.cubemap.image,
                                                   envmap.cubemap.view,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   env_params.layer,
                                                   {0, 1, 1, 0},
                                                   image_size(),
                                                   env_params.mip);

                                ImGui::TableSetColumnIndex(1);
                                auto conv_params = ImGuiWidgets::ImageControls("Convolution", 1.0F, 5.0F);

                                editor.image_array(envmap.convolution.image,
                                                   envmap.convolution.view,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   conv_params.layer,
                                                   {0, 1, 1, 0},
                                                   image_size(),
                                                   conv_params.mip);

                                ImGui::TableSetColumnIndex(2);
                                auto pref_params = ImGuiWidgets::ImageControls(
                                    "Prefiltered", static_cast<f32>(shader_constants::kEnvPrefilterMips - 1), 5.0F);

                                editor.image_array(envmap.prefiltered.image,
                                                   envmap.prefiltered.view,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   pref_params.layer,
                                                   {0, 1, 1, 0},
                                                   image_size(),
                                                   pref_params.mip);

                                ImGui::EndTable();
                            }
                        }

                        if (ImGui::CollapsingHeader("Directional light controls", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            glm::vec3 euler = glm::degrees(glm::eulerAngles(sun_transform.rotation));
                            if (ImGui::DragFloat3("Direction", glm::value_ptr(euler)))
                                sun_transform.rotation = glm::quat(glm::radians(euler));

                            ImGui::DragFloat("Intensity (lm/m^2)", &sun_data.intensity);
                            ImGui::DragFloat("Camera exposure", &camera_exposure);
                            ImGui::ColorEdit3(
                                "Color", &sun_data.rgb_color.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
                        }

                        if (ImGui::CollapsingHeader("Camera controls", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            client_scene.get_view<entt::entity, camera_component>().each(
                                [&](entt::entity id, camera_component& camera_comp)
                                {
                                    cpp::stack_string name;
                                    bool selected = camera == id;

                                    if (const auto comp_id = client_scene.try_get_component<id_component>(id))
                                    {
#if !defined(NDEBUG)
                                        name = comp_id->name;
#else
                                        name = cpp::stack_string::make_formatted("%ull", comp_id->id);
#endif
                                    }
                                    else
                                    {
                                        name = cpp::stack_string::make_formatted("unknown camera #%d",
                                                                                 static_cast<int>(id));
                                    }

                                    if (ImGui::TreeNodeEx(name.c_str(), selected ? ImGuiTreeNodeFlags_DefaultOpen : 0))
                                    {
                                        if (ImGui::Checkbox("Selected", &selected) && selected)
                                        {
                                            camera = client_scene.create_ref(id);
                                        }

                                        auto euler = glm::degrees(camera_comp.horizontal_fov);
                                        ImGui::DragFloat("FOV", &euler);
                                        camera_comp.horizontal_fov = glm::radians(euler);

                                        ImGui::DragFloat("Near plane", &camera_comp.near_plane);
                                        ImGui::TreePop();
                                    }
                                });
                        }

                        if (ImGui::CollapsingHeader("Depth pyramid"))
                        {
                            auto env_params = ImGuiWidgets::ImageControls(
                                "Pyramid", static_cast<f32>(depth_pyramid.pyramid_count - 1));

                            const auto size_x = ImGui::GetContentRegionAvail().x;
                            const auto size_y = ImGui::GetContentRegionAvail().x / camera_data.aspect_ratio;

                            editor.image(depth_pyramid.image.image,
                                         depth_pyramid.image.view,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         {0, 1, 1, 0},
                                         {size_x, size_y},
                                         env_params.mip,
                                         camera_data.near_plane);
                        }

                        if (ImGui::CollapsingHeader("Shadow map"))
                        {
                            ImGui::SliderFloat("Lambda", &csm.split_lambda, 0.0F, 1.0F);
                            ImGui::DragFloat("Max distance", &csm.max_range);

                            auto env_params = ImGuiWidgets::ImageControls(
                                "Shadow map", 1.0F, static_cast<f32>(shader_constants::kMaxShadowCascades - 1));

                            const auto size_x = ImGui::GetContentRegionAvail().x;
                            const auto size_y = ImGui::GetContentRegionAvail().x / camera_data.aspect_ratio;

                            editor.depth_image(csm.shadow_map.image,
                                               csm.cascade_views[static_cast<i32>(env_params.layer)],
                                               VK_IMAGE_LAYOUT_GENERAL,
                                               {0, 1, 1, 0},
                                               {size_x, size_y},
                                               env_params.mip,
                                               camera_data.near_plane);
                        }
                    }

                    ImGui::End();
                    editor.end_frame(m_renderer);
                }
#endif

                app_cmd_transition_image(m_rhi, buffer, *frame_image, rhi::image_layout::present);
#if !NO_PERF_QUERY
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool.handle, 1);
#endif

                m_rhi.cmd_end_recording(buffer);
                m_rhi.present(buffer,
                              *swapchain,
                              *m_rhi.query_queue(*context, rhi::queue_kind::gfx),
                              *m_rhi.query_queue(*context, rhi::queue_kind::present));

#if !NO_PERF_QUERY
                vkDeviceWaitIdle(m_renderer.get_context().device);

                const auto frame_stats =
                    query_frame_statistics_data(m_renderer.get_context().device, timestamp_query_pool);

                u32 tris_total_reported = 0;
                pipeline_statistics_data.clear();
                for (u32 i = 0; i < pipeline_statistics_query.index; ++i)
                {
                    pipeline_statistics_data.emplace_back(
                        query_pipeline_statistics_data(m_renderer.get_context().device, pipeline_statistics_query, i));
                    tris_total_reported += pipeline_statistics_data.back().triangles_count;
                }

                VkPhysicalDeviceProperties props = {};
                vkGetPhysicalDeviceProperties(m_renderer.get_context().physical_device, &props);

                profile_data.update(static_cast<f64>(frame_stats.frame_start) * props.limits.timestampPeriod * 1e-6,
                                    static_cast<f64>(frame_stats.frame_end) * props.limits.timestampPeriod * 1e-6,
                                    tris_total_reported,
                                    scene_info.triangles);

                TracyPlotConfig("Total GPU time", tracy::PlotFormatType::Number, false, true, 0);
                TracyPlot("Total GPU time", profile_data.gpu_render_time);

                TracyPlotConfig("Fraction tris drawn", tracy::PlotFormatType::Percentage, false, true, 0);
                TracyPlot("Fraction tris drawn", profile_data.tris_from_max * 100.0);

                TracyPlotConfig("Total tris drawn", tracy::PlotFormatType::Number, false, true, 0);
                TracyPlot("Total tris drawn", static_cast<i64>(profile_data.tris_in_scene_total));

                const auto str = cpp::stack_string::make_formatted("CPU: %.3lfms; GPU: %.3lfms; Tris/s (B): %lf",
                                                                   dt * 1000.0F,
                                                                   profile_data.gpu_render_time,
                                                                   profile_data.tris_per_second);
                SDL_SetWindowTitle(m_window.get_native_handle().window, str.c_str());
#endif

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

    csm.shutdown(m_rhi, *context);
    envmap.shutdown(m_rhi, *context);

    watcher.shutdown();
    pipelines.shutdown(m_rhi, *context);

    m_rhi.destroy_image(*context, depth_image);
    m_rhi.destroy_image(*context, render_target);
    m_rhi.destroy_image(*context, vis_buffer);
    destroy_depth_pyramid(m_rhi, depth_pyramid, *context);

#if !NO_PERF_QUERY
    platform::vk_destroy_query_pool(m_renderer.get_context().device, timestamp_query_pool);
    platform::vk_destroy_query_pool(m_renderer.get_context().device, pipeline_statistics_query);
#endif

    m_rhi.destroy_buffer(*context, geometry_pool.vertex);
    m_rhi.destroy_buffer(*context, geometry_pool.meshlets);
    m_rhi.destroy_buffer(*context, geometry_pool.primitives);
    m_rhi.destroy_buffer(*context, geometry_pool.instances);
    m_rhi.destroy_buffer(*context, geometry_pool.materials);
    m_rhi.destroy_buffer(*context, geometry_pool.meshlets_payload);

    m_rhi.destroy_buffer(*context, draw_count_buffer);
    m_rhi.destroy_buffer(*context, indexed_count_buffer);
    m_rhi.destroy_buffer(*context, indexed_indices_buffer);
    m_rhi.destroy_buffer(*context, mesh_visibility_buffer);
    m_rhi.destroy_buffer(*context, meshlets_visibility_buffer);
    m_rhi.destroy_buffer(*context, indexed_draw_indirect_buffer);
    m_rhi.destroy_buffer(*context, meshlets_draw_indirect_buffer);

    m_rhi.destroy_bindless_set(*context, *bindless_textures_desc_set);

    for (auto& buffer : frame_cull_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }
    for (auto& buffer : world_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }
    for (auto& buffer : shadow_cascades_data_buffers)
    {
        m_rhi.destroy_buffer(*context, buffer.buffer);
    }

    m_rhi.destroy_sampler(*context, *color_sampler);
    m_rhi.destroy_sampler(*context, *depth_texture_sampler);
    m_rhi.destroy_sampler(*context, *shadow_alpha_sampler);
    m_rhi.destroy_sampler(*context, *bindless_textures_sampler);

    for (auto& texture : textures)
    {
        m_rhi.destroy_image(*context, texture);
    }

    return 0;
}
