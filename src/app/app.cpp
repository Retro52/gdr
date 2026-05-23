#include <volk.h>

#include <types.hpp>

#include <app/app.hpp>
#include <app/argv.hpp>
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
    using frame_cull_data = shader_types::FrameCullData;

    struct render_pc_data
    {
        glm::mat4 vp;
        vec3 sun_direction;
        vec3 sun_color;
    };

    void build_frustum(frame_cull_data& data, const glm::mat4& iproj, const glm::mat4& iview)
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

    loader::stats populate_scene(const u32 draw_count, const cpp::heap_array<mesh::raw_mesh>& primitives, scene& scene,
                                 render::vk_scene_geometry_pool& geometry_pool)
    {
        ZoneScoped;

        const u32 kVolumeItemsPerSide = std::lroundl(std::cbrt(draw_count));

        loader::loader_context ctx;
        cpp::heap_array<loader::prim_layout> layouts(primitives.size());

        u64 total_vertices = 0;
        u64 total_indices  = 0;
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
                layout.lod_array[i] = {total_indices + (geometry_pool.index.offset / sizeof(u32)),
                                       total_meshlets + (geometry_pool.meshlets.offset / sizeof(loader::meshlet)),
                                       total_payload + geometry_pool.meshlets_payload.offset};

                total_indices += lod.raw_indices.size();
                total_meshlets += lod.raw_meshlets.size();
                total_payload += lod.raw_meshlets_payload.size();
            }
        }

        ctx.primitives.resize(primitives.size());
        ctx.vertices.resize(total_vertices);
        ctx.indices.resize(total_indices);
        ctx.meshlets.resize(total_meshlets);
        ctx.meshlets_data.resize(total_payload);

        for (u32 i = 0; i < primitives.size(); ++i)
        {
            loader::encode_raw_mesh(ctx, primitives[i], layouts[i]);
        }

        auto& mat          = ctx.materials.emplace_back();
        mat.diffuse_factor = vec4(0.9, 0.4, 0.9, 1.0);

        render::upload_data(geometry_pool.transfer, geometry_pool.index, ctx.indices.data(), ctx.indices.size());
        render::upload_data(
            geometry_pool.transfer, geometry_pool.primitives, ctx.primitives.data(), ctx.primitives.size());
        render::upload_data(geometry_pool.transfer, geometry_pool.vertex, ctx.vertices.data(), ctx.vertices.size());
        render::upload_data(geometry_pool.transfer, geometry_pool.meshlets, ctx.meshlets.data(), ctx.meshlets.size());
        render::upload_data(
            geometry_pool.transfer, geometry_pool.materials, ctx.materials.data(), ctx.materials.size());
        render::upload_data(
            geometry_pool.transfer, geometry_pool.meshlets_payload, ctx.meshlets_data.data(), ctx.meshlets_data.size());

        auto* instances_ptr = static_cast<loader::instance*>(geometry_pool.transfer.mapped);

        u64 triangles_max     = 0;
        u64 visibility_offset = 0;

        for (u32 i = 0; i < draw_count; ++i)
        {
            ZoneScopedN("create models within the scene");

            const u32 id_random = get_random<i32>(0, primitives.size() - 1);

            auto entity = scene.create_entity();
            entity.add_component<id_component>();

            auto& instance = instances_ptr[i];
            vec3 position  = {
                i % kVolumeItemsPerSide,
                (i / kVolumeItemsPerSide) % kVolumeItemsPerSide,
                i / (kVolumeItemsPerSide * kVolumeItemsPerSide),
            };

            constexpr f32 kDensityInverse = 7.5F;
            position *= vec3(1.5F);
            position *= vec3(get_random<f32>(-kDensityInverse, kDensityInverse),
                             get_random<f32>(-kDensityInverse, kDensityInverse),
                             get_random<f32>(-kDensityInverse, kDensityInverse));

            instance.material_index    = 0;
            instance.visibility_offset = visibility_offset;
            instance.mesh_data_index   = layouts[id_random].prim_index;

            instance.pos_and_scale = {position, get_random<f32>(0.75F, 10.0F)};
            instance.rotation_quat =
                glm::quat(vec3(get_random<f32>(-180, 180), get_random<f32>(-180, 180), get_random<f32>(-180, 180)));

            triangles_max += loader::get_max_lod_tris(ctx.primitives[id_random]);
            visibility_offset += loader::get_max_lod_meshlets(ctx.primitives[id_random]);
        }

        render::submit_transfer(geometry_pool.transfer,
                                geometry_pool.instances.buffer,
                                VkBufferCopy {.size = draw_count * sizeof(loader::instance)});

        return {.meshes     = primitives.size(),
                .meshlets   = visibility_offset,
                .triangles  = triangles_max,
                .primitives = draw_count};
    }
}

render::vk_renderer create_vk_renderer(window& app_window)
{
    ZoneScoped;

    constexpr auto features_table = render::rendering_features_table()
#if !defined(NDEBUG)
                                        .request(render::rendering_features_table::eValidation)
#endif
                                        .request(render::rendering_features_table::eMeshShading)
                                        .request(render::rendering_features_table::ePipelineStats)
                                        .require(render::rendering_features_table::e8BitIntegers)
                                        .require(render::rendering_features_table::e16BitFloats)
                                        .require(render::rendering_features_table::eDrawIndirect)
                                        .require(render::rendering_features_table::eDynamicRender)
                                        .require(render::rendering_features_table::eSamplerMinMax)
                                        .require(render::rendering_features_table::eBindlessTextures)
                                        .require(render::rendering_features_table::eScalarBlockLayout)
                                        .require(render::rendering_features_table::eSynchronization2);
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

    depth_pyramid_data depth_pyramid = create_depth_pyramid(m_window.get_size_in_px(),
                                                            VK_FORMAT_R32_SFLOAT,
                                                            m_renderer.get_context().device,
                                                            m_renderer.get_context().allocator);

    bool exit = false;
    bool mesh_shading_supported =
        m_renderer.get_context().enabled_device_features.supported(render::rendering_features_table::eMeshShading);
    bool pipeline_stats_supported =
        m_renderer.get_context().enabled_device_features.supported(render::rendering_features_table::ePipelineStats);

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
        render::vk_image& depth_image;
        depth_pyramid_data& depth_pyramid;
    } resize_ctx(m_renderer, depth_image, depth_pyramid);

    m_events_queue.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& ctx = *static_cast<resize_context*>(user_data);

            vkDeviceWaitIdle(ctx.renderer.get_context().device);
            ctx.renderer.resize_swapchain(payload.window.size_px);

            render::destroy_image(
                ctx.renderer.get_context().device, ctx.renderer.get_context().allocator, ctx.depth_image);

            ctx.depth_image = create_depth_image(payload.window.size_px,
                                                 ctx.renderer.get_swapchain().depth_format,
                                                 ctx.renderer.get_context().device,
                                                 ctx.renderer.get_context().allocator);

            destroy_depth_pyramid(
                ctx.depth_pyramid, ctx.renderer.get_context().device, ctx.renderer.get_context().allocator);
            ctx.depth_pyramid = create_depth_pyramid(payload.window.size_px,
                                                     VK_FORMAT_R32_SFLOAT,
                                                     ctx.renderer.get_context().device,
                                                     ctx.renderer.get_context().allocator);
        },
        &resize_ctx);

    render::vk_descriptor_set bindless_textures_desc_set =
        *render::create_bindless_textures_set(m_renderer.get_context().device, 65536);
    VkSampler bindless_textures_sampler = *render::create_sampler(m_renderer.get_context().device,
                                                                  VK_FILTER_LINEAR,
                                                                  VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                                                  VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                                  VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);

    pso_data pipelines;
    pipelines.load(m_renderer, bindless_textures_desc_set);

#if !NO_EDITOR
    imgui_layer editor(m_window, m_renderer);
#endif

    render::vk_scene_geometry_pool geometry_pool {
        .index      = render::vk_shared_buffer(m_renderer, 128_MB, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        .vertex     = render::vk_shared_buffer(m_renderer, 128_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .primitives = render::vk_shared_buffer(m_renderer, 1_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .instances  = render::vk_shared_buffer(m_renderer, 48_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .materials  = render::vk_shared_buffer(m_renderer, 1_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),

        .transfer = *render::create_buffer_transfer(m_renderer.get_context().device,
                                                    m_renderer.get_context().allocator,
                                                    m_renderer.get_context().queues[render::queue_kind::eTransfer],
                                                    128_MB)};

    if (mesh_shading_supported)
    {
        geometry_pool.meshlets = render::vk_shared_buffer(m_renderer, 16_MB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        geometry_pool.meshlets_payload =
            render::vk_shared_buffer(m_renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }

    loader::stats stats;
    cpp::heap_array<render::vk_image> textures;

    app::argv_handler argv_handler(argc, argv);
    const int instance_count = argv_handler.read_numeric("--instances");
    const int first_instance = argv_handler.get_positional_args_start();

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

        stats = populate_scene(instance_count, meshes, client_scene, geometry_pool);
    }
    else
    {
        stats = loader::load_scene(argv[1], client_scene, m_renderer, geometry_pool, textures);
    }

    entity camera = client_scene.empty();
    if (const auto loaded_camera = client_scene.get_view<entt::entity, camera_component>().front();
        loaded_camera != entt::null)
    {
        camera = client_scene.create_ref(loaded_camera);
    }

    entity editor_camera = client_scene.create_entity();
    editor_camera.add_component<id_component>(DEBUG_ONLY(id_component("editor camera")));
    editor_camera.add_component<transform_component>();
    editor_camera.add_component<camera_component>(camera_component {
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(90.0F),
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
        sun.add_component<directional_light_component>(vec3(1));

        auto& t    = sun.emplace_component<transform_component>();
        t.rotation = glm::quat(vec3(0));
    }

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

    render::vk_query pipeline_statistics_query;
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

    render::vk_buffer draw_count_buffer = *render::create_buffer(
        sizeof(u32[3]),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer mesh_visibility_buffer = *render::create_buffer(
        (stats.primitives + 31) / 8,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer meshlets_visibility_buffer = *render::create_buffer(
        (stats.meshlets + 31) / 8,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::fill_buffer(geometry_pool.transfer, mesh_visibility_buffer, 0_u8);
    render::fill_buffer(geometry_pool.transfer, meshlets_visibility_buffer, 0_u8);

    render::vk_buffer indexed_draw_indirect_buffer = *render::create_buffer(
        16 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_buffer meshlets_draw_indirect_buffer = *render::create_buffer(
        16 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        m_renderer.get_context().allocator,
        0);

    render::vk_mapped_buffer frame_cull_data_buffers[3];
    for (u32 i = 0; i < 3; i++)
    {
        frame_cull_data_buffers[i] =
            *render::create_buffer_mapped(sizeof(frame_cull_data),
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                          m_renderer.get_context().allocator,
                                          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    }

    gpu_profile_data profile_data;
    render_settings client_render_settings;
    pipeline_statistics_data pipeline_stats_early;
    pipeline_statistics_data pipeline_stats_late;

    glm::mat4 camera_proj;
    glm::mat4 camera_view;
    glm::mat4 camera_proj_view;
    glm::mat4 debug_camera_view;

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

    render::debug::frustum_renderer frustum_renderer(m_renderer);

#if !NO_EDITOR
    editor::hierarchy_window_context hierarchy_window_context;
    editor::info_widget_context info_widget_context {
        .m_camera        = controller,
        .m_gpu_profile   = profile_data,
        .m_geometry_pool = geometry_pool,
    };
#endif

    auto draw_scene = [&](VkCommandBuffer cmd, const render::vk_pipeline& pipeline, VkBuffer frame_cull_data)
    {
        ZoneScopedN("app.instance.run.draw_scene");

        constexpr u32 kMaxSetZeroBindings = 12;
        render::vk_descriptor_info render_bindings[kMaxSetZeroBindings];

        u32 curr_bind  = 0;
        auto bind_next = [&](const render::vk_descriptor_info& next)
        {
            assert2(curr_bind < kMaxSetZeroBindings);
            render_bindings[curr_bind++] = next;
        };

        bind_next(geometry_pool.materials.buffer.buffer);
        bind_next(render::vk_descriptor_info(bindless_textures_sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED));
        bind_next(geometry_pool.vertex.buffer.buffer);

        if (enable_meshlets_pipeline)
        {
            bind_next(geometry_pool.meshlets.buffer.buffer);
            bind_next(geometry_pool.meshlets_payload.buffer.buffer);
            bind_next(geometry_pool.primitives.buffer.buffer);
            bind_next(geometry_pool.instances.buffer.buffer);
            bind_next(meshlets_draw_indirect_buffer.buffer);
            bind_next(frame_cull_data);
            bind_next(meshlets_visibility_buffer.buffer);
            bind_next(
                render::vk_descriptor_info(depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL));

            pipeline.push_descriptor_set(cmd, render_bindings);
            pipeline.bind_descriptor_set(cmd, bindless_textures_desc_set);

            vkCmdDrawMeshTasksIndirectEXT(cmd, draw_count_buffer.buffer, 0, 1, 0);
        }
        else
        {
            bind_next(geometry_pool.instances.buffer.buffer);
            bind_next(indexed_draw_indirect_buffer.buffer);

            pipeline.push_descriptor_set(cmd, render_bindings);
            pipeline.bind_descriptor_set(cmd, bindless_textures_desc_set);

            vkCmdBindIndexBuffer(cmd, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexedIndirectCount(cmd,
                                          indexed_draw_indirect_buffer.buffer,
                                          0,
                                          draw_count_buffer.buffer,
                                          0,
                                          stats.primitives,
                                          sizeof(shader_types::DrawIndexedIndirect));
        }
    };

    auto render_loop = [&]()
    {
        ZoneScopedN("app.instance.run.render_loop");
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update(camera_transform, camera_data, static_cast<f32>(dt));

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

                    frame_cull_data fcd {.pyramid_size  = depth_pyramid.base_size,
                                         .viewport_size = m_window.get_size_in_px(),
                                         .draw_count    = static_cast<u32>(stats.primitives),
                                         .flags         = client_render_settings.flags};
                    build_frustum(fcd, projection, view);
                    (*static_cast<frame_cull_data*>(frame_cull_data_buffer.mapped)) = fcd;
                }
                else
                {
                    static_cast<frame_cull_data*>(frame_cull_data_buffer.mapped)->view  = debug_camera_view;
                    static_cast<frame_cull_data*>(frame_cull_data_buffer.mapped)->flags = client_render_settings.flags;
                }

                camera_proj = camera_data.get_projection_matrix();
                camera_view = camera_component::get_view_matrix(camera_transform.position, camera_transform.rotation);
                camera_proj_view = camera_proj * camera_view;

                auto& sun_transform = sun.get_component<transform_component>();
                auto& sun_data      = sun.get_component<directional_light_component>();

                auto sun_direction = glm::normalize(glm::mat3_cast(sun_transform.rotation) * vec3(0, 1, 0));

                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull last frame occluders"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {geometry_pool.primitives.buffer.buffer,
                                                                             geometry_pool.instances.buffer.buffer,
                                                                             draw_count_buffer.buffer,
                                                                             mesh_visibility_buffer.buffer,
                                                                             frame_cull_data_buffer.buffer,
                                                                             enable_meshlets_pipeline
                                                                                 ? meshlets_draw_indirect_buffer.buffer
                                                                                 : indexed_draw_indirect_buffer.buffer};

                    const render::vk_pipeline& cull_pass = enable_meshlets_pipeline
                                                             ? pipelines[pso_id::task_cull_pipeline]
                                                             : pipelines[pso_id::indexed_cull_pipeline];
                    cull_pass.bind(buffer);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, static_cast<u32>(stats.primitives), 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                render::transition_image(buffer,
                                         m_renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL);

                render::transition_image(buffer,
                                         depth_image.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_ASPECT_DEPTH_BIT);
                begin_rendering(buffer,
                                m_renderer.get_frame_swapchain_image().image_view,
                                depth_image.view,
                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                m_renderer.get_scissor());

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                pipeline_statistics_query.begin(buffer, 0, 0);

                {
                    const auto& render_pipeline = enable_meshlets_pipeline ? pipelines[pso_id::task_render_pipeline]
                                                                           : pipelines[pso_id::indexed_render_pipeline];
                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(
                        buffer,
                        render_pc_data {freeze_cull_data ? camera_proj * debug_camera_view : camera_proj_view,
                                        sun_direction,
                                        sun_data.rgb_color});

                    ZoneScopedN("draw last frame occluders");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "draw last frame occluders"));

                    draw_scene(buffer, render_pipeline, frame_cull_data_buffer.buffer);
                }

                pipeline_statistics_query.end(buffer, 0);
                vkCmdEndRendering(buffer);

                // Reduce the depth buffer pyramid
                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "depth reduce"));

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
                                                  VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                    }
                }

                if (freeze_cull_data)
                {
                    ZoneScopedN("draw last frame occluders from an actual perspective");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(),
                                           buffer,
                                           "draw last frame occluders from an actual perspective"));

                    begin_rendering(buffer,
                                    m_renderer.get_frame_swapchain_image().image_view,
                                    depth_image.view,
                                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    m_renderer.get_scissor());

                    const auto& render_pipeline = enable_meshlets_pipeline ? pipelines[pso_id::task_render_pipeline]
                                                                           : pipelines[pso_id::indexed_render_pipeline];
                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(buffer,
                                                  render_pc_data {camera_proj_view, sun_direction, sun_data.rgb_color});

                    draw_scene(buffer, render_pipeline, frame_cull_data_buffer.buffer);
                    vkCmdEndRendering(buffer);
                }

                {
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "cull new objects"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        geometry_pool.primitives.buffer.buffer,
                        geometry_pool.instances.buffer.buffer,
                        draw_count_buffer.buffer,
                        mesh_visibility_buffer.buffer,
                        frame_cull_data_buffer.buffer,
                        enable_meshlets_pipeline ? meshlets_draw_indirect_buffer.buffer
                                                 : indexed_draw_indirect_buffer.buffer,
                        render::vk_descriptor_info(
                            depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL)};

                    const render::vk_pipeline& cull_pass = enable_meshlets_pipeline
                                                             ? pipelines[pso_id::task_occlusion_cull_pipeline]
                                                             : pipelines[pso_id::indexed_cull_occlusion_pipeline];

                    cull_pass.bind(buffer);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, static_cast<u32>(stats.primitives), 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                {
                    const auto& render_pipeline = enable_meshlets_pipeline
                                                    ? pipelines[pso_id::task_render_late_pipeline]
                                                    : pipelines[pso_id::indexed_render_pipeline];
                    render_pipeline.bind(buffer);
                    render_pipeline.push_constant(buffer,
                                                  render_pc_data {camera_proj_view, sun_direction, sun_data.rgb_color});

                    ZoneScopedN("draw new objects");
                    TRACY_ONLY(TracyVkZone(m_renderer.get_frame_tracy_context(), buffer, "draw new objects"));

                    begin_rendering(buffer,
                                    m_renderer.get_frame_swapchain_image().image_view,
                                    depth_image.view,
                                    VK_ATTACHMENT_LOAD_OP_LOAD,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    m_renderer.get_scissor());

                    pipeline_statistics_query.begin(buffer, 1, 0);
                    draw_scene(buffer, render_pipeline, frame_cull_data_buffer.buffer);
                    pipeline_statistics_query.end(buffer, 1);

                    if (freeze_cull_data)
                    {
                        frustum_renderer.draw(buffer, camera_proj_view, frame_cull_data_buffer);
                    }

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
                        info_widget_context.draw("Pipeline stats [early]", pipeline_stats_early);
                        info_widget_context.draw("Pipeline stats [late]", pipeline_stats_late);
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

                        if (ImGui::CollapsingHeader("Directional light controls", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            glm::vec3 euler = glm::degrees(glm::eulerAngles(sun_transform.rotation));
                            ImGui::DragFloat3("Direction", glm::value_ptr(euler));
                            sun_transform.rotation = glm::quat(glm::radians(euler));

                            ImGui::ColorEdit3("Color", &sun_data.rgb_color.x);
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

                        if (ImGui::CollapsingHeader("Render targets", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            static int img_in_line = 2;
                            ImGui::SliderInt("Images in line", &img_in_line, 1, 2);

                            const auto size_x = ImGui::GetContentRegionAvail().x / static_cast<f32>(img_in_line);
                            const auto size_y = (ImGui::GetContentRegionAvail().x / static_cast<f32>(img_in_line))
                                              / camera_data.aspect_ratio;

                            editor.depth_image(depth_image.image,
                                               depth_image.view,
                                               VK_IMAGE_LAYOUT_GENERAL,
                                               {0, 1, 1, 0},
                                               {size_x, size_y});
                            if (img_in_line > 1)
                            {
                                ImGui::SameLine();
                            }

                            editor.image(m_renderer.get_frame_swapchain_image().image,
                                         m_renderer.get_frame_swapchain_image().image_view,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         {0, 1, 1, 0},
                                         {size_x, size_y});
                        }

                        if (ImGui::CollapsingHeader("Depth pyramid"))
                        {
                            static int idx = 0;
                            idx            = std::min(idx, static_cast<int>(depth_pyramid.pyramid_count) - 1);

                            ImGui::SliderInt("Index", &idx, 0, static_cast<int>(depth_pyramid.pyramid_count) - 1);

                            const auto size_x = ImGui::GetContentRegionAvail().x;
                            const auto size_y = ImGui::GetContentRegionAvail().x / camera_data.aspect_ratio;

                            editor.image(depth_pyramid.image.image,
                                         depth_pyramid.views[idx],
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         {0, 1, 1, 0},
                                         {size_x, size_y},
                                         1.0F);
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

                auto frame_stats = query_frame_statistics_data(m_renderer.get_context().device, timestamp_query_pool);

                pipeline_stats_early =
                    query_pipeline_statistics_data(m_renderer.get_context().device, pipeline_statistics_query, 0);
                pipeline_stats_late =
                    query_pipeline_statistics_data(m_renderer.get_context().device, pipeline_statistics_query, 1);

                VkPhysicalDeviceProperties props = {};
                vkGetPhysicalDeviceProperties(m_renderer.get_context().physical_device, &props);

                profile_data.update(static_cast<f64>(frame_stats.frame_start) * props.limits.timestampPeriod * 1e-6,
                                    static_cast<f64>(frame_stats.frame_end) * props.limits.timestampPeriod * 1e-6,
                                    pipeline_stats_early.triangles_count + pipeline_stats_late.triangles_count,
                                    stats.triangles);

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

    frustum_renderer.shutdown(m_renderer);

    pipelines.shutdown(m_renderer);
    render::destroy_image(m_renderer.get_context().device, m_renderer.get_context().allocator, depth_image);
    destroy_depth_pyramid(depth_pyramid, m_renderer.get_context().device, m_renderer.get_context().allocator);

    render::destroy_query_pool(m_renderer.get_context().device, timestamp_query_pool);
    render::destroy_query_pool(m_renderer.get_context().device, pipeline_statistics_query);

    render::destroy_command_buffer(m_renderer.get_context().device, geometry_pool.transfer.staging_command_buffer);

    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.index.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.vertex.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.meshlets.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.primitives.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.instances.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.materials.buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.meshlets_payload.buffer);

    vmaUnmapMemory(m_renderer.get_context().allocator, geometry_pool.transfer.staging_buffer.allocation);
    render::destroy_buffer(m_renderer.get_context().allocator, geometry_pool.transfer.staging_buffer);

    render::destroy_buffer(m_renderer.get_context().allocator, draw_count_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, mesh_visibility_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, meshlets_visibility_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, indexed_draw_indirect_buffer);
    render::destroy_buffer(m_renderer.get_context().allocator, meshlets_draw_indirect_buffer);

    render::destroy_descriptor_set(m_renderer.get_context().device, bindless_textures_desc_set);

    for (auto& buffer : frame_cull_data_buffers)
    {
        render::destroy_buffer_mapped(m_renderer.get_context().allocator, buffer);
    }

    vkDestroySampler(m_renderer.get_context().device, bindless_textures_sampler, nullptr);
    for (auto& texture : textures)
    {
        render::destroy_image(m_renderer.get_context().device, m_renderer.get_context().allocator, texture);
    }

    return 0;
}
