#include <volk.h>

#include <types.hpp>

#include <app/app.hpp>
#include <app/argv.hpp>
#include <app/environment.hpp>
#include <app/gpu_stats.hpp>
#include <app/pso.hpp>
#include <app/render.hpp>
#include <camera_controller.hpp>
#include <codegen/render_settings.hpp>
#include <editor/hierarchy.hpp>
#include <editor/info.hpp>
#include <events.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui/gpu_profile_data.hpp>
#include <imgui/imex.hpp>
#include <imgui/imgui_layer.hpp>
#include <imgui/imwidgets.hpp>
#include <render/debug/frustum_renderer.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_query.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/loader.hpp>
#include <scene/mesh_load.hpp>
#include <scene/scene.hpp>
#include <tracy/Tracy.hpp>
#include <window.hpp>

#define NO_EDITOR     0
#define NO_PERF_QUERY 0

namespace
{
    REGISTER_ENUM(debug_mode, shaded, lit, lit_diffuse, lit_ambient, lit_specular, uv, normal, tangent, world_pos,
                  color, metallic, roughness, albedo_texture, normal_texture, omr_texture, triangle_id, instance_id,
                  triangle_face);

    REGISTER_ENUM(cubemap_face, XP, XN, YP, YN, ZP, ZN);

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

    loader::scene_info populate_scene(const u32 draw_count, const cpp::heap_array<mesh::raw_mesh>& primitives,
                                      scene& scene, render::vk_scene_geometry_pool& geometry_pool)
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

        render::upload_data(
            geometry_pool.transfer, geometry_pool.primitives, ctx.primitives.data(), ctx.primitives.size());
        render::upload_data(geometry_pool.transfer, geometry_pool.vertex, ctx.vertices.data(), ctx.vertices.size());
        render::upload_data(geometry_pool.transfer, geometry_pool.meshlets, ctx.meshlets.data(), ctx.meshlets.size());
        render::upload_data(
            geometry_pool.transfer, geometry_pool.materials, ctx.materials.data(), ctx.materials.size());
        render::upload_data(
            geometry_pool.transfer, geometry_pool.meshlets_payload, ctx.meshlets_data.data(), ctx.meshlets_data.size());
        render::upload_data(geometry_pool.transfer, geometry_pool.instances, instances.data(), instances.size());

        return {.meshes           = primitives.size(),
                .meshlets         = visibility_offset,
                .triangles        = triangles_max,
                .primitives       = draw_count,
                .mat_offset_table = mat_offset_table};
    }
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

static const render::vk_pipeline& get_render_pipeline(app::pso_data& pipelines, u32 material_class, bool occlusion_cull,
                                                      bool enable_meshlets)
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

render::vk_renderer create_vk_renderer(window& app_window)
{
    ZoneScoped;

    constexpr auto features_table = render::rendering_features_table()
#if !defined(NDEBUG)
                                        .request(render::feature_flag::eValidation)
#endif
#if !NO_PERF_QUERY
                                        .request(render::feature_flag::ePipelineStats)
#endif
#if !defined(__APPLE__)
                                        .require(render::feature_flag::eSamplerMinMax)
#endif
                                        .request(render::feature_flag::eMeshShading)
                                        .require(render::feature_flag::e8BitIntegers)
                                        .require(render::feature_flag::e16BitTypes)
                                        .require(render::feature_flag::eDrawIndirect)
                                        .require(render::feature_flag::eDynamicRender)
                                        .require(render::feature_flag::eBindlessTextures)
                                        .require(render::feature_flag::eScalarBlockLayout)
                                        .require(render::feature_flag::eSynchronization2);
    return {
        render::instance_desc {
                               .app_name        = "Vulkan renderer",
                               .app_version     = 1,
                               .device_features = features_table,
                               },
        app_window,
        false
    };
}

app::instance::instance()
    : m_window("VK window", {1920, 960}, false)
    , m_events_queue(m_window)
    , m_renderer(create_vk_renderer(m_window))
{
}

int app::instance::run(const int argc, char* argv[])
{
    if (argc < 2)
    {
        return -1;
    }

    render::vk_image depth_image = create_depth_image(m_window.get_size_in_px(),
                                                      m_renderer.get_swapchain().depth_format,
                                                      m_renderer.get_context().device,
                                                      m_renderer.get_context().allocator);

    render::vk_image render_target = create_color_image(m_window.get_size_in_px(),
                                                        m_renderer.get_swapchain().surface_format.format,
                                                        m_renderer.get_context().device,
                                                        m_renderer.get_context().allocator);

    vis_buffer_data vis_buffer = create_vis_buffer_data(
        m_window.get_size_in_px(), m_renderer.get_context().device, m_renderer.get_context().allocator);

    depth_pyramid_data depth_pyramid = create_depth_pyramid(m_window.get_size_in_px(),
                                                            VK_FORMAT_R32_SFLOAT,
                                                            m_renderer.get_context().device,
                                                            m_renderer.get_context().allocator);

    app::environment environment {
        m_renderer, VK_FORMAT_R16G16B16A16_SFLOAT, {1024, 512, 128, 32}
    };

    bool exit = false;
    bool mesh_shading_supported =
        m_renderer.get_context().enabled_device_features.supported(render::feature_flag::eMeshShading);
    bool pipeline_stats_supported =
        m_renderer.get_context().enabled_device_features.supported(render::feature_flag::ePipelineStats);

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
        render::vk_renderer& renderer;

        vis_buffer_data& vis_buffer;
        render::vk_image& depth_image;
        render::vk_image& render_target;
        depth_pyramid_data& depth_pyramid;
    } resize_ctx(m_renderer, vis_buffer, depth_image, render_target, depth_pyramid);

    m_events_queue.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& ctx = *static_cast<resize_context*>(user_data);

            vkDeviceWaitIdle(ctx.renderer.get_context().device);
            ctx.renderer.resize_swapchain(payload.window.size_px);

            render::destroy_image(
                ctx.renderer.get_context().device, ctx.renderer.get_context().allocator, ctx.depth_image);
            render::destroy_image(
                ctx.renderer.get_context().device, ctx.renderer.get_context().allocator, ctx.render_target);

            ctx.depth_image = create_depth_image(payload.window.size_px,
                                                 ctx.renderer.get_swapchain().depth_format,
                                                 ctx.renderer.get_context().device,
                                                 ctx.renderer.get_context().allocator);

            ctx.render_target = create_color_image(payload.window.size_px,
                                                   ctx.renderer.get_swapchain().surface_format.format,
                                                   ctx.renderer.get_context().device,
                                                   ctx.renderer.get_context().allocator);

            destroy_depth_pyramid(
                ctx.depth_pyramid, ctx.renderer.get_context().device, ctx.renderer.get_context().allocator);
            ctx.depth_pyramid = create_depth_pyramid(payload.window.size_px,
                                                     VK_FORMAT_R32_SFLOAT,
                                                     ctx.renderer.get_context().device,
                                                     ctx.renderer.get_context().allocator);

            render::destroy_image(
                ctx.renderer.get_context().device, ctx.renderer.get_context().allocator, ctx.vis_buffer.vis_buffer_img);

            ctx.vis_buffer = create_vis_buffer_data(
                payload.window.size_px, ctx.renderer.get_context().device, ctx.renderer.get_context().allocator);
        },
        &resize_ctx);

    render::vk_descriptor_set bindless_textures_desc_set =
        *render::create_bindless_textures_set(m_renderer.get_context().device, 65536);
    VkSampler bindless_textures_sampler = *render::create_sampler(m_renderer.get_context().device,
                                                                  VK_FILTER_LINEAR,
                                                                  VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                                                  VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                                  VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE,
                                                                  16.0F);

    VkSampler color_sampler = *render::create_sampler(m_renderer.get_context().device,
                                                      VK_FILTER_LINEAR,
                                                      VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    VkSampler depth_texture_sampler = *render::create_sampler(m_renderer.get_context().device,
                                                              VK_FILTER_NEAREST,
                                                              VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                              VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

    pso_data pipelines;
    pipelines.load(m_renderer, bindless_textures_desc_set);

#if !NO_EDITOR
    imgui_layer editor(m_window, m_renderer, pipelines);
#endif

    render::vk_scene_geometry_pool geometry_pool {
        .vertex           = render::vk_shared_buffer(m_renderer, 128_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .meshlets         = render::vk_shared_buffer(m_renderer, 16_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .primitives       = render::vk_shared_buffer(m_renderer, 1_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .instances        = render::vk_shared_buffer(m_renderer, 48_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .materials        = render::vk_shared_buffer(m_renderer, 48_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .meshlets_payload = render::vk_shared_buffer(m_renderer, 128_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),

        .transfer = *render::create_buffer_transfer(m_renderer.get_context().device,
                                                    m_renderer.get_context().allocator,
                                                    m_renderer.get_context().queues[render::queue_kind::eTransfer],
                                                    128_MB),
    };

    loader::scene_info scene_info;
    cpp::heap_array<render::vk_image> textures;

    app::argv_handler argv_handler(argc, argv);
    const int instance_count = argv_handler.read_numeric("--instances");
    const int first_instance = argv_handler.get_positional_args_start();
    auto env_map             = argv_handler.read_string<fs::path_string>("--environment");

    assert2(instance_count == 0 || first_instance > 0);

    // test scene stuff
    scene client_scene;
    if (instance_count > 0 && first_instance > 0)
    {
        cpp::heap_array<mesh::raw_mesh> meshes;
        for (int i = first_instance; i < argc; ++i)
        {
            auto ctx = loader::load_meshes(argv[i]);
            if (!ctx)
            {
                continue;
            }

            meshes.append(ctx->primitives);
        }

        scene_info = populate_scene(instance_count, meshes, client_scene, geometry_pool);
    }
    else
    {
        scene_info = loader::load_scene(argv[first_instance], client_scene, m_renderer, geometry_pool, textures);
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

    sun.get_component<transform_component>().rotation = glm::quat(glm::radians(argv_handler.read_vec3(
        "--sun_direction", glm::eulerAngles(sun.get_component<transform_component>().rotation))));

    for (u32 i = 0; i < textures.size(); ++i)
    {
        auto& tex = textures[i];
        if (tex.image == VK_NULL_HANDLE)
        {
            continue;
        }

        const VkDescriptorImageInfo img_info = {.imageView = tex.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

        const VkWriteDescriptorSet desc_write {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = bindless_textures_desc_set.descriptor_set,
            .dstBinding      = 0,
            .dstArrayElement = i + 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo      = &img_info,
        };

        vkUpdateDescriptorSets(m_renderer.get_context().device, 1, &desc_write, 0, nullptr);
    }

    constexpr u32 kQueryPoolCount = 64;
    render::vk_query timestamp_query_pool =
        *render::create_query_pool(m_renderer.get_context().device, kQueryPoolCount, VK_QUERY_TYPE_TIMESTAMP);

    u32 pipeline_statistics_query_index = 0;
    render::vk_query pipeline_statistics_query;
#if !NO_PERF_QUERY
    if (pipeline_stats_supported)
    {
        pipeline_statistics_query =
            *render::create_pipeline_stat_query_pool(m_renderer.get_context().device,
                                                     kQueryPoolCount,
                                                     VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT
                                                         | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT
                                                         | VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
                                                         | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT
                                                         | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT);
    }
#endif

    render::vk_buffer draw_count_buffer = *render::create_buffer(
        sizeof(u32[shader_constants::kMatClassCount * 3]),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer indexed_count_buffer = *render::create_buffer(
        sizeof(u32[2]),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer mesh_visibility_buffer = *render::create_buffer(
        (scene_info.primitives + 31) / 8,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer meshlets_visibility_buffer = *render::create_buffer(
        (scene_info.meshlets + 31) / 8,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::fill_buffer(geometry_pool.transfer, mesh_visibility_buffer, 0_u8);
    render::fill_buffer(geometry_pool.transfer, meshlets_visibility_buffer, 0_u8);

    render::vk_buffer indexed_indices_buffer = *render::create_buffer(
        96_MB,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer indexed_draw_indirect_buffer = *render::create_buffer(
        16_MB,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer meshlets_draw_indirect_buffer = *render::create_buffer(
        16_MB,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    cpp::heap_array<render::vk_mapped_buffer> world_data_buffers(m_renderer.get_frames_in_flight());
    cpp::heap_array<render::vk_mapped_buffer> frame_cull_data_buffers(m_renderer.get_frames_in_flight());

    for (u32 i = 0; i < m_renderer.get_frames_in_flight(); i++)
    {
        world_data_buffers[i] = *render::create_buffer_mapped(sizeof(shader_types::FrameWorldData),
                                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                              m_renderer.get_context().allocator,
                                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

        frame_cull_data_buffers[i] =
            *render::create_buffer_mapped(sizeof(shader_types::FrameCullData),
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                          m_renderer.get_context().allocator,
                                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    gpu_profile_data profile_data;
    render_settings client_render_settings;
    cpp::heap_array<pipeline_statistics_data> pipeline_statistics_data;

    glm::mat4 camera_proj;
    glm::mat4 camera_view;
    glm::mat4 camera_proj_view;
    glm::mat4 debug_camera_view;

    u32 draw_materials_mask = 0xFFFF;
    debug_mode draw_debug_mode {debug_mode::shaded};

    f32 camera_exposure             = 10.0F;
    f32 environment_compensation_ev = 0.0f;
    f32 environment_intensity       = 1250.0f;

    bool freeze_cull_data         = false;
    bool enable_vsync             = m_renderer.get_vsync();
    bool enable_fullscreen        = m_window.get_fullscreen();
    bool enable_meshlets_pipeline = mesh_shading_supported;

    auto get_time = []<typename T = f64>()
    {
        return static_cast<T>(SDL_GetPerformanceCounter()) / static_cast<T>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    camera_controller controller(m_events_queue, camera);

    render::debug::frustum_renderer frustum_renderer(pipelines[pso_id::frustum_debug]);

#if !NO_EDITOR
    editor::hierarchy_window_context hierarchy_window_context;
    editor::info_widget_context info_widget_context {
        .m_camera        = controller,
        .m_gpu_profile   = profile_data,
        .m_geometry_pool = geometry_pool,
    };
#endif

    const auto offset_table = make_offset_table(scene_info.mat_offset_table);
    auto fill_indexed       = [&](VkCommandBuffer cmd, const u32 material_class, const bool enable_occlusion_cull)
    {
        if (enable_meshlets_pipeline)
        {
            return;
        }

        TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "build index buffer late"));

        app::zero_buffer(cmd, indexed_count_buffer, sizeof(u32));
#if defined(__APPLE__)
        app::zero_buffer(cmd, indexed_draw_indirect_buffer);
#endif

        render::vk_descriptor_bindings bindings;
        bindings.bind(geometry_pool.meshlets.buffer.buffer)
            .bind(geometry_pool.meshlets_payload.buffer.buffer)
            .bind(geometry_pool.primitives.buffer.buffer)
            .bind(geometry_pool.instances.buffer.buffer)
            .bind(meshlets_draw_indirect_buffer.buffer)
            .bind(frame_cull_data_buffers[m_renderer.get_frame_index()].buffer)
            .bind(indexed_indices_buffer.buffer)
            .bind(indexed_draw_indirect_buffer.buffer)
            .bind(indexed_count_buffer.buffer)
            .bind(meshlets_visibility_buffer.buffer);

        if (enable_occlusion_cull)
        {
            bindings.bind(
                render::vk_descriptor_info(depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL));
        }

        pso_id id;
        switch (material_class)
        {
        case shader_constants::kMatClassMasked :
        case shader_constants::kMatClassTranslucent :
            id = enable_occlusion_cull ? pso_id::indexed_fill_ds_late_pipeline : pso_id::indexed_fill_ds_pipeline;
            break;
        default :
        case shader_constants::kMatClassOpaque :
            id = enable_occlusion_cull ? pso_id::indexed_fill_late_pipeline : pso_id::indexed_fill_pipeline;
            break;
        }

        const render::vk_pipeline& fill_pass = pipelines[id];

        fill_pass.bind(cmd);
        fill_pass.push_constant(cmd, offset_table[material_class]);
        fill_pass.push_descriptor_set(cmd, bindings.get());

        vkCmdDispatchIndirect(cmd, draw_count_buffer.buffer, material_class * 3 * sizeof(u32));

        render::cmd_stage_barrier(cmd,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                                      | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                  VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT
                                      | VK_ACCESS_2_INDEX_READ_BIT);
    };

    auto draw_scene = [&](VkCommandBuffer cmd,
                          const render::vk_pipeline& pipeline,
                          const u32 material_class,
                          const VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_LOAD)
    {
        ZoneScopedN("app.instance.run.draw_scene");
        TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "draw scene"));

        constexpr VkPipelineStageFlags2 kAttachmentStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
                                                          | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                                          | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        constexpr VkAccessFlags2 kAttachmentAccess =
            VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
            | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        render::cmd_stage_barrier(cmd, kAttachmentStages, kAttachmentAccess, kAttachmentStages, kAttachmentAccess);

        begin_rendering(cmd,
                        vis_buffer.vis_buffer_img.view,
                        depth_image.view,
                        load_op,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        m_renderer.get_scissor());
        pipeline_statistics_query.begin(cmd, pipeline_statistics_query_index, 0);

        render::vk_descriptor_bindings bindings;
        bindings.bind(geometry_pool.vertex.buffer.buffer);
        bindings.bind(geometry_pool.materials.buffer.buffer);
        bindings.bind(render::vk_descriptor_info(bindless_textures_sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED));
        bindings.bind(geometry_pool.meshlets.buffer.buffer);
        bindings.bind(geometry_pool.meshlets_payload.buffer.buffer);
        bindings.bind(geometry_pool.primitives.buffer.buffer);
        bindings.bind(geometry_pool.instances.buffer.buffer);

        if (enable_meshlets_pipeline)
        {
            bindings.bind(meshlets_draw_indirect_buffer.buffer);
            bindings.bind(frame_cull_data_buffers[m_renderer.get_frame_index()].buffer);
            bindings.bind(meshlets_visibility_buffer.buffer);
            bindings.bind(
                render::vk_descriptor_info(depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL));

            pipeline.push_descriptor_set(cmd, bindings.get());
            pipeline.bind_descriptor_set(cmd, bindless_textures_desc_set);

            vkCmdDrawMeshTasksIndirectEXT(cmd, draw_count_buffer.buffer, material_class * 3 * sizeof(u32), 1, 0);
        }
        else
        {
            bindings.bind(indexed_draw_indirect_buffer.buffer);

            pipeline.push_descriptor_set(cmd, bindings.get());
            pipeline.bind_descriptor_set(cmd, bindless_textures_desc_set);

            vkCmdBindIndexBuffer(cmd, indexed_indices_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

#if defined(__APPLE__)
            vkCmdDrawIndexedIndirect(cmd,
                                     indexed_draw_indirect_buffer.buffer,
                                     0,
                                     scene_info.mat_offset_table[material_class],
                                     sizeof(shader_types::DrawIndexedIndirect));
#else
            vkCmdDrawIndexedIndirectCount(cmd,
                                          indexed_draw_indirect_buffer.buffer,
                                          0,
                                          indexed_count_buffer.buffer,
                                          sizeof(u32),
                                          scene_info.mat_offset_table[material_class],
                                          sizeof(shader_types::DrawIndexedIndirect));
#endif
        }

        pipeline_statistics_query.end(cmd, pipeline_statistics_query_index++);
        vkCmdEndRendering(cmd);
    };

    m_renderer.submit(
        [&](VkCommandBuffer cmd)
        {
            ZoneScopedN("app.instance.run.preload");

            do
            {
            } while (!m_renderer.acquire_frame());
            constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};

            const auto viewport = m_renderer.get_viewport();
            const auto scissor  = m_renderer.get_scissor();

            vkBeginCommandBuffer(cmd, &command_buffer_begin_info);
            vkCmdSetScissor(cmd, 0, 1, &scissor);
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            environment.init(pipelines, m_renderer);
            if (!env_map.empty())
            {
                TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), cmd, "load environment"));
                environment.load(env_map, pipelines, m_renderer, geometry_pool.transfer);
            }

            render::transition_image(cmd,
                                     m_renderer.get_frame_swapchain_image().image,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_2_NONE,
                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                     VK_ACCESS_2_NONE);

            vkEndCommandBuffer(cmd);
            m_renderer.present_frame(cmd);
        });

    auto render_loop = [&]()
    {
        ZoneScopedN("app.instance.run.render_loop");
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update(static_cast<f32>(dt));

        if (m_renderer.get_vsync() != enable_vsync)
        {
            m_renderer.set_vsync(enable_vsync);
            return;
        }

        if (!m_renderer.acquire_frame())
        {
            return;
        }

        m_renderer.submit(
            [&](const VkCommandBuffer buffer)
            {
                ZoneScopedN("main.m_renderer.submit");
                constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};

                const auto viewport = m_renderer.get_viewport();
                const auto scissor  = m_renderer.get_scissor();

                vkBeginCommandBuffer(buffer, &command_buffer_begin_info);
                TRACY_ONLY(TracyVkCollect(m_renderer.get_frame_tracy_context(), buffer));

                timestamp_query_pool.reset(buffer, 0, kQueryPoolCount);

                pipeline_statistics_query_index = 0;
                pipeline_statistics_query.reset(buffer, 0, kQueryPoolCount);

                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool.handle, 0);

                auto& frame_cull_data_buffer = frame_cull_data_buffers[m_renderer.get_frame_index()];
                if (!freeze_cull_data)
                {
                    auto projection = client_render_settings.render_distance > 0
                                        ? camera_data.get_projection_matrix(client_render_settings.render_distance)
                                        : camera_data.get_projection_matrix();

                    auto view = camera_component::get_view_matrix(camera_transform.position, camera_transform.rotation);
                    debug_camera_view = view;

                    shader_types::FrameCullData fcd {.pyramid_size  = depth_pyramid.base_size,
                                                     .viewport_size = m_window.get_size_in_px(),
                                                     .draw_count    = static_cast<u32>(scene_info.primitives),
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

                auto& world_data_buffer = world_data_buffers[m_renderer.get_frame_index()];
                (*static_cast<shader_types::FrameWorldData*>(world_data_buffer.mapped)) = shader_types::FrameWorldData {
                    .sun_color         = {sun_data.rgb_color, sun_data.intensity},
                    .camera_pos        = camera_transform.position,
                    .debug_mode        = static_cast<u32>(draw_debug_mode),
                    .sun_direction     = sun_direction,
                    .camera_exposure   = camera_exposure,
                    .environment_scale = environment_intensity * glm::exp2(environment_compensation_ev),
                };

                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull last frame occluders"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {geometry_pool.primitives.buffer.buffer,
                                                                             geometry_pool.instances.buffer.buffer,
                                                                             geometry_pool.materials.buffer.buffer,
                                                                             draw_count_buffer.buffer,
                                                                             mesh_visibility_buffer.buffer,
                                                                             frame_cull_data_buffer.buffer,
                                                                             meshlets_draw_indirect_buffer.buffer};

                    const render::vk_pipeline& cull_pass = pipelines[pso_id::task_cull_pipeline];

                    cull_pass.bind(buffer);
                    cull_pass.push_constant(buffer, offset_table);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, static_cast<u32>(scene_info.primitives), 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                render::transition_image(
                    buffer, render_target.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                render::transition_image(buffer,
                                         m_renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL);

                render::transition_image(
                    buffer, vis_buffer.vis_buffer_img.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                render::transition_image(buffer,
                                         depth_image.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_ASPECT_DEPTH_BIT);

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                app::zero_buffer(buffer, indexed_count_buffer, 0);
                for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                {
                    if (!(draw_materials_mask & (1 << i)))
                    {
                        continue;
                    }

                    fill_indexed(buffer, i, false);
                    const auto& render_pipeline = get_render_pipeline(pipelines, i, false, enable_meshlets_pipeline);

                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(
                        buffer,
                        shader_types::DrawPushConstants {
                            freeze_cull_data ? camera_proj * debug_camera_view : camera_proj_view, offset_table[i]});

                    vkCmdSetCullMode(
                        buffer, i == shader_constants::kMatClassOpaque ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
                    draw_scene(
                        buffer, render_pipeline, i, i == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD);
                }

                // Reduce the depth buffer pyramid
                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "depth reduce"));

                    render::cmd_stage_barrier(buffer,
                                              VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                                                  | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

                    render::transition_image(
                        buffer, depth_pyramid.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                    const auto& depth_reduce_pipeline = pipelines[pso_id::depth_reduce_pipeline];
                    depth_reduce_pipeline.bind(buffer);

                    for (i32 i = 0; i < depth_pyramid.pyramid_count; ++i)
                    {
                        const render::vk_descriptor_info cull_pass_bindings[] = {
                            render::vk_descriptor_info(depth_pyramid.sampler,
                                                       i == 0 ? depth_image.view : depth_pyramid.views[i - 1],
                                                       VK_IMAGE_LAYOUT_GENERAL),
                            render::vk_descriptor_info(
                                depth_pyramid.sampler, depth_pyramid.views[i], VK_IMAGE_LAYOUT_GENERAL),
                        };

                        depth_reduce_pipeline.push_descriptor_set(buffer, cull_pass_bindings);

                        const ivec2 out_size = glm::max(depth_pyramid.base_size >> i, ivec2(1));
                        depth_reduce_pipeline.push_constant(buffer, vec2(out_size));
                        depth_reduce_pipeline.dispatch(buffer, out_size.x, out_size.y, 1);

                        render::cmd_stage_barrier(buffer,
                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                  VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                  VK_ACCESS_2_SHADER_SAMPLED_READ_BIT
                                                      | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                    }
                }

                // NOTE: only executed if freeze_cull_data == true
                for (u32 i = 0; i < shader_constants::kMatClassCount && freeze_cull_data; ++i)
                {
                    if (!(draw_materials_mask & (1 << i)))
                    {
                        continue;
                    }

                    fill_indexed(buffer, i, false);
                    const auto& render_pipeline = get_render_pipeline(pipelines, i, false, enable_meshlets_pipeline);

                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(buffer,
                                                  shader_types::DrawPushConstants(camera_proj_view, offset_table[i]));

                    vkCmdSetCullMode(
                        buffer, i == shader_constants::kMatClassOpaque ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
                    draw_scene(
                        buffer, render_pipeline, i, i == 0 ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD);
                }

                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull new objects"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        geometry_pool.primitives.buffer.buffer,
                        geometry_pool.instances.buffer.buffer,
                        geometry_pool.materials.buffer.buffer,
                        draw_count_buffer.buffer,
                        mesh_visibility_buffer.buffer,
                        frame_cull_data_buffer.buffer,
                        meshlets_draw_indirect_buffer.buffer,
                        render::vk_descriptor_info(
                            depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL)};

                    const render::vk_pipeline& cull_pass = pipelines[pso_id::task_occlusion_cull_pipeline];

                    cull_pass.bind(buffer);
                    cull_pass.push_constant(buffer, offset_table);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, static_cast<u32>(scene_info.primitives), 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
                            | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                for (u32 i = 0; i < shader_constants::kMatClassCount; ++i)
                {
                    if (!((draw_materials_mask)
                    // if (!((draw_materials_mask & ~static_cast<u64>(1 << shader_constants::kMatClassTranslucent))
                          & (1 << i)))
                    {
                        continue;
                    }

                    fill_indexed(buffer, i, true);
                    const auto& render_pipeline = get_render_pipeline(pipelines, i, true, enable_meshlets_pipeline);

                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(buffer,
                                                  shader_types::DrawPushConstants(camera_proj_view, offset_table[i]));

                    vkCmdSetCullMode(
                        buffer, i == shader_constants::kMatClassOpaque ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
                    draw_scene(buffer, render_pipeline, i);
                }

                {
                    ZoneScopedN("Resolve pass");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "vb resolve"));

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

                    const render::vk_descriptor_info resolve_pass_bindings[] = {
                        render::vk_descriptor_info(VK_NULL_HANDLE, render_target.view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(
                            VK_NULL_HANDLE, vis_buffer.vis_buffer_img.view, VK_IMAGE_LAYOUT_GENERAL),
                        indexed_indices_buffer.buffer,
                        geometry_pool.vertex.buffer.buffer,
                        geometry_pool.meshlets.buffer.buffer,
                        geometry_pool.meshlets_payload.buffer.buffer,
                        geometry_pool.primitives.buffer.buffer,
                        geometry_pool.instances.buffer.buffer,
                        geometry_pool.materials.buffer.buffer,
                        world_data_buffer.buffer,
                        render::vk_descriptor_info(
                            bindless_textures_sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED),
                        render::vk_descriptor_info(depth_texture_sampler, depth_image.view, VK_IMAGE_LAYOUT_GENERAL),
                        environment.get_lut_descriptor_info(),
                        environment.get_cube_descriptor_info(),
                        environment.get_conv_descriptor_info(),
                        environment.get_pref_descriptor_info(),
                    };

                    const render::vk_pipeline& resolve_pass =
                        pipelines[enable_meshlets_pipeline ? pso_id::mesh_resolve_pipeline
                                                           : pso_id::vert_resolve_pipeline];

                    resolve_pass.bind(buffer);
                    resolve_pass.push_descriptor_set(buffer, resolve_pass_bindings);
                    resolve_pass.bind_descriptor_set(buffer, bindless_textures_desc_set);

                    resolve_pass.push_constant(buffer,
                                               shader_types::ResolvePassPushConstants(camera_view, camera_proj));

                    resolve_pass.dispatch(buffer, m_window.get_size_in_px().x, m_window.get_size_in_px().y, 1);

                    render::cmd_stage_barrier(buffer,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
                }

                {
                    ZoneScopedN("FXAA pass");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "vb resolve"));

                    const render::vk_descriptor_info fxaa_pass_bindings[] = {
                        render::vk_descriptor_info(
                            VK_NULL_HANDLE, m_renderer.get_frame_swapchain_image().image_view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(color_sampler, render_target.view, VK_IMAGE_LAYOUT_GENERAL),
                        render::vk_descriptor_info(depth_texture_sampler, depth_image.view, VK_IMAGE_LAYOUT_GENERAL),
                    };

                    const render::vk_pipeline& fxaa_pass = pipelines[pso_id::fxaa_pipeline];

                    fxaa_pass.bind(buffer);
                    fxaa_pass.push_descriptor_set(buffer, fxaa_pass_bindings);

                    fxaa_pass.push_constant(buffer, camera_data.near_plane);

                    fxaa_pass.dispatch(buffer, m_window.get_size_in_px().x, m_window.get_size_in_px().y, 1);

                    render::cmd_stage_barrier(buffer,
                                              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                              VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                              VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT
                                                  | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
                }

                if (freeze_cull_data)
                {
                    ZoneScopedN("Frustum debug render pass");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "frustum debug"));

                    begin_rendering(buffer,
                                    m_renderer.get_frame_swapchain_image().image_view,
                                    depth_image.view,
                                    VK_ATTACHMENT_LOAD_OP_LOAD,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    m_renderer.get_scissor());

                    frustum_renderer.draw(buffer, camera_proj_view, frame_cull_data_buffer);
                    vkCmdEndRendering(buffer);
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
                                environment.load(env_map, pipelines, m_renderer, geometry_pool.transfer);
                            }

                            constexpr ImGuiTableFlags flags =
                                ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame;

                            ImGui::DragFloat("Env intensity", &environment_intensity);
                            ImGui::DragFloat("Env compensation", &environment_compensation_ev);

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

                                editor.image_array(environment.cubemap.image,
                                                   environment.cubemap.view,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   env_params.layer,
                                                   {0, 1, 1, 0},
                                                   image_size(),
                                                   env_params.mip);

                                ImGui::TableSetColumnIndex(1);
                                auto conv_params = ImGuiWidgets::ImageControls("Convolution", 1.0F, 5.0F);

                                editor.image_array(environment.convolution.image,
                                                   environment.convolution.view,
                                                   VK_IMAGE_LAYOUT_GENERAL,
                                                   conv_params.layer,
                                                   {0, 1, 1, 0},
                                                   image_size(),
                                                   conv_params.mip);

                                ImGui::TableSetColumnIndex(2);
                                auto pref_params = ImGuiWidgets::ImageControls(
                                    "Prefiltered", static_cast<f32>(shader_constants::kEnvPrefilterMips - 1), 5.0F);

                                editor.image_array(environment.prefiltered.image,
                                                   environment.prefiltered.view,
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
                            ImGui::DragFloat3("Direction", glm::value_ptr(euler));
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
                    }

                    ImGui::End();
                    editor.end_frame(m_renderer);
                }
#endif

                render::transition_image(buffer,
                                         m_renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                         VK_PIPELINE_STAGE_2_NONE,
                                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_ACCESS_2_NONE);

                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool.handle, 1);

                vkEndCommandBuffer(buffer);
                m_renderer.present_frame(buffer);

#if !NO_PERF_QUERY
                vkDeviceWaitIdle(m_renderer.get_context().device);

                const auto frame_stats =
                    query_frame_statistics_data(m_renderer.get_context().device, timestamp_query_pool);

                u32 tris_total_reported = 0;
                pipeline_statistics_data.clear();
                for (u32 i = 0; i < pipeline_statistics_query_index; ++i)
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

    vkDeviceWaitIdle(m_renderer.get_context().device);

    pipelines.shutdown(m_renderer);
    environment.shutdown(m_renderer);

    render::destroy_image(m_renderer.get_context().device, m_renderer.get_context().allocator, depth_image);
    render::destroy_image(m_renderer.get_context().device, m_renderer.get_context().allocator, render_target);
    render::destroy_image(
        m_renderer.get_context().device, m_renderer.get_context().allocator, vis_buffer.vis_buffer_img);
    destroy_depth_pyramid(depth_pyramid, m_renderer.get_context().device, m_renderer.get_context().allocator);

    render::destroy_query_pool(m_renderer.get_context().device, timestamp_query_pool);
    render::destroy_query_pool(m_renderer.get_context().device, pipeline_statistics_query);

    render::destroy_command_buffer(m_renderer.get_context().device, geometry_pool.transfer.staging_command_buffer);

    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.vertex.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.meshlets.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.primitives.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.instances.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.materials.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.meshlets_payload.buffer);

    vmaUnmapMemory(m_renderer.get_context().allocator, geometry_pool.transfer.staging_buffer.allocation);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.transfer.staging_buffer);

    render::destroy_buffer(m_renderer.get_context().allocator, draw_count_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, indexed_count_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, indexed_indices_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, mesh_visibility_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, meshlets_visibility_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, indexed_draw_indirect_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, meshlets_draw_indirect_buffer);

    render::destroy_descriptor_set(m_renderer.get_context().device, bindless_textures_desc_set);

    for (auto& buffer : frame_cull_data_buffers)
    {
        render::destroy_buffer_mapped(m_renderer.get_context().allocator, buffer);
    }
    for (auto& buffer : world_data_buffers)
    {
        render::destroy_buffer_mapped(m_renderer.get_context().allocator, buffer);
    }

    vkDestroySampler(m_renderer.get_context().device, color_sampler, nullptr);
    vkDestroySampler(m_renderer.get_context().device, depth_texture_sampler, nullptr);
    vkDestroySampler(m_renderer.get_context().device, bindless_textures_sampler, nullptr);
    for (auto& texture : textures)
    {
        render::destroy_image(m_renderer.get_context().device, m_renderer.get_context().allocator, texture);
    }

    return 0;
}
