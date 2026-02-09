#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

#include <assert2.hpp>
#include <camera_controller.hpp>
#include <codegen/camera_controller.hpp>
#include <codegen/imgui/gpu_profile_data.hpp>
#include <codegen/render_settings.hpp>
#include <codegen/scene/components.hpp>
#include <events.hpp>
#include <fs/fs.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui/gpu_profile_data.hpp>
#include <imgui/imex.hpp>
#include <imgui/imgui_layer.hpp>
#include <render/debug/frustum_renderer.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_query.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <tracy/Tracy.hpp>
#include <window.hpp>

#include <iostream>
#include <vector>

#define NO_EDITOR          0
#define NO_PERF_QUERY      0
#define TEST_MULTI_OBJECTS 0

struct pc_data
{
    glm::mat4 pv;
};

struct draw_task_indirect_cmd
{
    u32 work_group_count[3];
    u32 base_meshlet;
    u32 mesh_id;
};

struct draw_indexed_indirect
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    i32 vertex_offset;
    u32 first_instance;
    u32 mesh_id;
};

struct frame_cull_data
{
    glm::mat4 view;
    float frustum[6];  // left/right/top/bottom/znear/zfar
    vec2 pyramid_size;
    float p00;
    float p11;
    u32 draw_count;
    u32 flags;

    frame_cull_data& build_frustum(const glm::mat4& iproj, const glm::mat4& iview)
    {
        this->view = iview;
        this->p00  = iproj[0][0];
        this->p11  = iproj[1][1];

        auto t_pv = glm::transpose(iproj);

        auto plane = [&](const vec4 eq)
        {
            return eq / glm::length(vec3(eq));
        };

        const vec4 hor_plane = plane(t_pv[3] + t_pv[0]);
        const vec4 ver_plane = plane(t_pv[3] + t_pv[1]);

        this->frustum[0] = hor_plane.x;
        this->frustum[1] = hor_plane.z;

        this->frustum[2] = glm::abs(ver_plane.y);
        this->frustum[3] = ver_plane.z;

        const auto w = t_pv[2].w;
        const auto z = glm::max(t_pv[2].z, 1e-9F);

        this->frustum[4] = w - z;
        this->frustum[5] = w / z;

        return *this;
    }
};

struct depth_image_data
{
    render::vk_image image;
};

struct depth_pyramid_data
{
    VkImageView views[12] {};
    render::vk_image image;
    VkSampler sampler;
    ivec2 base_size;
    u32 pyramid_count {0};
};

void begin_rendering(VkCommandBuffer cmd, VkImageView color, VkImageView depth, VkAttachmentLoadOp load_op,
                     VkAttachmentStoreOp store_op, const VkRect2D& vp)
{
    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .renderArea = vp, .layerCount = 1};

    VkRenderingAttachmentInfo color_attachment_info {};
    VkRenderingAttachmentInfo depth_attachment_info {};
    if (color != VK_NULL_HANDLE)
    {
        color_attachment_info = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = color,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .loadOp      = load_op,
            .storeOp     = store_op,
            .clearValue  = {
                            .color = {0.0F, 0.0F, 0.0F, 1.0F},
                            }
        };

        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments    = &color_attachment_info;
    }

    if (depth != VK_NULL_HANDLE)
    {
        depth_attachment_info = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = depth,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .loadOp      = load_op,
            .storeOp     = store_op,
            .clearValue  = {
                            .depthStencil = {0.0F, 0},
                            }
        };

        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(cmd, &rendering_info);
}

void reset_draw_count_buffer(VkCommandBuffer cmd, const render::vk_buffer& draw_count_buffer)
{
    vkCmdFillBuffer(cmd, draw_count_buffer.buffer, 0, draw_count_buffer.size, 0);
    render::cmd_buffer_barrier(cmd,
                               draw_count_buffer.buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

void draw_scene(const bool use_meshlets, VkCommandBuffer cmd, const render::vk_pipeline& pipeline,
                const render::vk_scene_geometry_pool& geometry_pool, const render::vk_buffer& meshes_data,
                const render::vk_buffer& meshes_transforms, const render::vk_buffer& draw_count_buffer,
                const render::vk_buffer& draw_indirect_cmds_buffer,
                const render::vk_mapped_buffer& frame_cull_data_buffer, u32 max_draws)
{
    if (use_meshlets)
    {
        const render::vk_descriptor_info render_bindings[] = {geometry_pool.vertex.buffer.buffer,
                                                              geometry_pool.meshlets.buffer.buffer,
                                                              geometry_pool.meshlets_payload.buffer.buffer,
                                                              meshes_data.buffer,
                                                              meshes_transforms.buffer,
                                                              draw_indirect_cmds_buffer.buffer,
                                                              frame_cull_data_buffer.buffer};

        pipeline.push_descriptor_set(cmd, render_bindings);
        vkCmdDrawMeshTasksIndirectCountEXT(cmd,
                                           draw_indirect_cmds_buffer.buffer,
                                           0,
                                           draw_count_buffer.buffer,
                                           0,
                                           max_draws,
                                           sizeof(draw_task_indirect_cmd));
    }
    else
    {
        const render::vk_descriptor_info render_bindings[] = {
            geometry_pool.vertex.buffer.buffer, meshes_transforms.buffer, draw_indirect_cmds_buffer.buffer};
        pipeline.push_descriptor_set(cmd, render_bindings);
        vkCmdBindIndexBuffer(cmd, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexedIndirectCount(cmd,
                                      draw_indirect_cmds_buffer.buffer,
                                      0,
                                      draw_count_buffer.buffer,
                                      0,
                                      max_draws,
                                      sizeof(draw_indexed_indirect));
    }
}

f64 bytes_to_mb(u64 bytes)
{
    return static_cast<f64>(bytes) / (1024.0 * 1024);
}

depth_image_data create_depth_image(const ivec2& size, const VkFormat format, VkDevice device, VmaAllocator allocator)
{
    const VkImageCreateInfo image_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    depth_image_data depth_image;
    depth_image.image = *render::create_image(device, image_create_info, VK_IMAGE_ASPECT_DEPTH_BIT, allocator);

    return depth_image;
}

void destroy_depth_pyramid(const render::vk_renderer& renderer, depth_pyramid_data& pyramid)
{
    for (u32 i = 0; i < pyramid.pyramid_count; ++i)
    {
        vkDestroyImageView(renderer.get_context().device, pyramid.views[i], nullptr);
    }

    pyramid.pyramid_count = 0;
    render::destroy_image(renderer.get_context().device, renderer.get_context().allocator, pyramid.image);
    vkDestroySampler(renderer.get_context().device, pyramid.sampler, nullptr);
}

depth_pyramid_data create_depth_pyramid(const ivec2& size, const VkFormat format, VkDevice device,
                                        VmaAllocator allocator)
{
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

    const VkImageCreateInfo image_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {static_cast<u32>(depth_pyramid.base_size.x), static_cast<u32>(depth_pyramid.base_size.y), 1},
        .mipLevels     = depth_pyramid.pyramid_count,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    depth_pyramid.image   = *render::create_image(device, image_create_info, VK_IMAGE_ASPECT_COLOR_BIT, allocator);
    depth_pyramid.sampler = *render::create_sampler(device,
                                                    VK_FILTER_LINEAR,
                                                    VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                    VK_SAMPLER_REDUCTION_MODE_MIN);

    for (u32 i = 0; i < depth_pyramid.pyramid_count; ++i)
    {
        depth_pyramid.views[i] =
            *render::create_image_view(device, depth_pyramid.image.image, format, VK_IMAGE_ASPECT_COLOR_BIT, i, 1);
    }

    return depth_pyramid;
}

cpp::stack_string format_big_number(u64 number)
{
    if (number < 1'000)
    {
        return cpp::stack_string::make_formatted("%d", number);
    }

    const char* magnitudes_per_thousand[] = {"K", "M", "B", "T"};

    auto magnitude     = static_cast<i32>(std::log10(number) / 3);
    const f64 fraction = static_cast<f64>(number) / glm::pow(1000, magnitude);
    return cpp::stack_string::make_formatted("%.3lf%s", fraction, magnitudes_per_thousand[magnitude - 1]);
}

void draw_shared_buffer_stats(const char* label, const render::vk_shared_buffer& buffer)
{
    ImGui::TextWrapped("%s buffer: %lf MB used (%lf MB total, %lf%%)",
                       label,
                       bytes_to_mb(buffer.offset),
                       bytes_to_mb(buffer.size),
                       static_cast<f64>(buffer.offset) * 100.0 / buffer.size);
}

f32 get_random_f32(const f32 min, const f32 max)
{
    return min + (static_cast<f32>(rand()) / RAND_MAX) * (max - min);
}

i32 get_random_i32(const i32 min, const i32 max)
{
    return min + static_cast<int>(get_random_f32(0.0F, 1.0F) * static_cast<f32>(max - min));
}

u64 populate_scene(const u32 draw_count, const char* models[], u32 models_count, scene& scene,
                   render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;

    u64 scene_triangles_total = 0;
    std::vector<static_model> unique_models;
    const u32 kVolumeItemsPerSide = std::cbrtl(draw_count);

    for (u32 i = 0; i < models_count; i++)
    {
        ZoneScopedN("load all models");
        auto&& loaded = *static_model::load(models[i], geometry_pool);
        unique_models.insert(unique_models.end(), loaded.begin(), loaded.end());
    }

    for (u32 i = 0; i < draw_count; ++i)
    {
        ZoneScopedN("create models within the scene");
        auto& model = unique_models[get_random_i32(0, models_count - 1)];
        scene_triangles_total += model.lod_array[0].indices_count / 3;

        auto entity = scene.create_entity();
        entity.add_component<static_model_component>(model);
        entity.add_component<id_component>();

        auto& transform = entity.emplace_component<transform_component>();

        transform.position = {
            i % kVolumeItemsPerSide,
            (i / kVolumeItemsPerSide) % kVolumeItemsPerSide,
            i / (kVolumeItemsPerSide * kVolumeItemsPerSide),
        };

#if TEST_MULTI_OBJECTS
        transform.position *=
            vec3(10.0F)
            * vec3(get_random_f32(-20.0F, 20.0F), get_random_f32(-20.0F, 20.0F), get_random_f32(-20.0F, 20.0F));
        transform.uniform_scale = get_random_f32(0.1F, 5.0F);
#else
        constexpr f32 kDensityInverse = 7.5F;
        transform.position *= vec3(1.5F);
        transform.position *= vec3(get_random_f32(-kDensityInverse, kDensityInverse),
                                   get_random_f32(-kDensityInverse, kDensityInverse),
                                   get_random_f32(-kDensityInverse, kDensityInverse));

        transform.uniform_scale = get_random_f32(0.75F, 10.0F);
        transform.rotation =
            glm::quat(vec3(get_random_f32(-180, 180), get_random_f32(-180, 180), get_random_f32(-180, 180)));
#endif
    }

    return scene_triangles_total;
}

void upload_draw_data(const render::vk_buffer_transfer& transfer, const render::vk_buffer& transform_buffer,
                      const render::vk_buffer& mesh_data_buffer, const scene& scene)
{
    ZoneScoped;

    auto&& view              = scene.get_view<transform_component, static_model_component>();
    const u64 view_size_hint = view.size_hint();

    auto* transforms    = static_cast<transform_component*>(transfer.mapped);
    auto* static_models = reinterpret_cast<static_model*>(static_cast<u8*>(transfer.mapped)
                                                          + sizeof(transform_component) * view_size_hint);

    u32 index = 0;
    view.each(
        [&](const transform_component& tc, const static_model_component& smc)
        {
            transforms[index]    = tc;
            static_models[index] = smc.model;

            ++index;
        });

    render::submit_transfer(transfer, transform_buffer, VkBufferCopy {.size = index * sizeof(transform_component)});
    render::submit_transfer(
        transfer,
        mesh_data_buffer,
        VkBufferCopy {.srcOffset = sizeof(transform_component) * view_size_hint, .size = index * sizeof(static_model)});
}

int main(int argc, char* argv[])
{
    srand(322);
    TracySetProgramName("gdr");

    window client_window("VK window", {1920, 960}, false);
    debug::assert2_set_window(client_window.get_native_handle().window);

    events_queue client_events(client_window);

    auto features_table = render::rendering_features_table()
#if !defined(NDEBUG)
                              .request(render::rendering_features_table::eValidation)
#endif
                              .request(render::rendering_features_table::eMeshShading)
                              .request(render::rendering_features_table::ePipelineStats)
                              .require(render::rendering_features_table::e8BitIntegers)
                              .require(render::rendering_features_table::eDrawIndirect)
                              .require(render::rendering_features_table::eDynamicRender)
                              .require(render::rendering_features_table::eSamplerMinMax)
                              .require(render::rendering_features_table::eSynchronization2);

    render::vk_renderer renderer(
        render::instance_desc {
            .app_name        = "Vulkan renderer",
            .app_version     = 1,
            .device_features = features_table,
        },
        client_window);
    depth_image_data depth_image = create_depth_image(client_window.get_size_in_px(),
                                                      renderer.get_swapchain().depth_format,
                                                      renderer.get_context().device,
                                                      renderer.get_context().allocator);

    depth_pyramid_data depth_pyramid = create_depth_pyramid(client_window.get_size_in_px(),
                                                            VK_FORMAT_R32_SFLOAT,
                                                            renderer.get_context().device,
                                                            renderer.get_context().allocator);

    bool exit = false;
    bool mesh_shading_supported =
        renderer.get_context().enabled_device_features.supported(render::rendering_features_table::eMeshShading);
    bool pipeline_stats_supported =
        renderer.get_context().enabled_device_features.supported(render::rendering_features_table::ePipelineStats);

    client_events.add_watcher(
        event_type::request_close,
        [](auto&, void* user_data)
        {
            *static_cast<bool*>(user_data) = true;
        },
        &exit);

    client_events.add_watcher(
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
        depth_image_data& depth_image;
        depth_pyramid_data& depth_pyramid;
    } resize_ctx(renderer, depth_image, depth_pyramid);

    client_events.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& ctx = *static_cast<resize_context*>(user_data);

            vkDeviceWaitIdle(ctx.renderer.get_context().device);
            ctx.renderer.resize_swapchain(payload.window.size_px);

            render::destroy_image(
                ctx.renderer.get_context().device, ctx.renderer.get_context().allocator, ctx.depth_image.image);

            ctx.depth_image = create_depth_image(payload.window.size_px,
                                                 ctx.renderer.get_swapchain().depth_format,
                                                 ctx.renderer.get_context().device,
                                                 ctx.renderer.get_context().allocator);

            destroy_depth_pyramid(ctx.renderer, ctx.depth_pyramid);
            ctx.depth_pyramid = create_depth_pyramid(payload.window.size_px,
                                                     VK_FORMAT_R32_SFLOAT,
                                                     ctx.renderer.get_context().device,
                                                     ctx.renderer.get_context().allocator);
        },
        &resize_ctx);

    render::vk_shader indexed_shaders[] = {
        *render::vk_shader::load(renderer, "../shaders/bin/mesh.vert.spv"),
        *render::vk_shader::load(renderer, "../shaders/bin/meshlets.frag.spv"),
    };

    render::vk_shader meshlets_shaders[] = {
        *render::vk_shader::load(renderer, "../shaders/bin/meshlets.task.spv"),
        *render::vk_shader::load(renderer, "../shaders/bin/meshlets.mesh.spv"),
        *render::vk_shader::load(renderer, "../shaders/bin/meshlets.frag.spv"),
    };

    const auto indexed_render_pipeline =
        *render::vk_pipeline::create_graphics(renderer, indexed_shaders, COUNT_OF(indexed_shaders));

    const auto indexed_cull_pipeline = *render::vk_pipeline::create_compute(
        renderer, *render::vk_shader::load(renderer, "../shaders/bin/cull_mesh.comp.spv"));

    const auto indexed_cull_occlusion_pipeline = *render::vk_pipeline::create_compute(
        renderer, *render::vk_shader::load(renderer, "../shaders/bin/cull_occlusion_mesh.comp.spv"));

    const auto depth_reduce_pipeline = *render::vk_pipeline::create_compute(
        renderer, *render::vk_shader::load(renderer, "../shaders/bin/depth_reduce.comp.spv"));

    render::vk_pipeline meshlets_cull_pipeline;
    render::vk_pipeline meshlets_render_pipeline;
    render::vk_pipeline meshlets_occlusion_cull_pipeline;
    if (mesh_shading_supported)
    {
        meshlets_render_pipeline =
            *render::vk_pipeline::create_graphics(renderer, meshlets_shaders, COUNT_OF(meshlets_shaders));

        meshlets_cull_pipeline = *render::vk_pipeline::create_compute(
            renderer, *render::vk_shader::load(renderer, "../shaders/bin/cull_meshlets.comp.spv"));

        meshlets_occlusion_cull_pipeline = *render::vk_pipeline::create_compute(
            renderer, *render::vk_shader::load(renderer, "../shaders/bin/cull_occlusion_meshlets.comp.spv"));
    }

#if !NO_EDITOR
    imgui_layer editor(client_window, renderer);
#endif

    // test scene stuff
    scene client_scene;
    auto camera = client_scene.create_entity();
    auto sun    = client_scene.create_entity();

    sun.add_component<id_component>(DEBUG_ONLY(id_component("sun")));
    sun.add_component<transform_component>();
    sun.add_component<directional_light_component>(
        directional_light_component {.color = vec3(1.0F, 1.0F, 1.0F), .direction = vec3(0.5)});

    camera.add_component<id_component>(DEBUG_ONLY(id_component("camera")));
    camera.add_component<transform_component>(transform_component {.position = vec3(0, 0, 55)});
    camera.add_component<camera_component>(camera_component {
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(90.0F),
    });

    render::vk_scene_geometry_pool geometry_pool {
        .index    = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
        .vertex   = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .transfer = *render::create_buffer_transfer(renderer.get_context().device,
                                                    renderer.get_context().allocator,
                                                    renderer.get_context().queues[render::queue_kind::eTransfer],
                                                    128 * 1024 * 1024)};

    if (mesh_shading_supported)
    {
        geometry_pool.meshlets =
            render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        geometry_pool.meshlets_payload =
            render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    }

    constexpr u32 kQueryPoolCount = 64;
    VkQueryPool timestamp_query_pool =
        *render::create_vk_query_pool(renderer.get_context().device, kQueryPoolCount, VK_QUERY_TYPE_TIMESTAMP);

    VkQueryPool pipeline_statistics_query = VK_NULL_HANDLE;
    if (pipeline_stats_supported)
    {
        pipeline_statistics_query = *render::create_vk_pipeline_stat_query_pool(
            renderer.get_context().device, kQueryPoolCount, VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT);
    }

    render::vk_buffer draw_count_buffer = *render::create_buffer(
        sizeof(u32),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    render::vk_buffer mesh_visibility_buffer = *render::create_buffer(
        32 * 1024 * 1024,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    render::fill_buffer(geometry_pool.transfer, mesh_visibility_buffer, 0);

    render::vk_buffer meshes_data =
        *render::create_buffer(32 * 1024 * 1024,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               renderer.get_context().allocator,
                               0);

    render::vk_buffer meshes_transforms =
        *render::create_buffer(16 * 1024 * 1024,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               renderer.get_context().allocator,
                               0);

    render::vk_buffer indexed_draw_indirect_buffer = *render::create_buffer(
        16 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    render::vk_buffer meshlets_draw_indirect_buffer = *render::create_buffer(
        16 * 1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    render::vk_mapped_buffer frame_cull_data_buffer =
        *render::create_buffer_mapped(sizeof(frame_cull_data),
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      renderer.get_context().allocator,
                                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    gpu_profile_data profile_data;
    render_settings client_render_settings;

    glm::mat4 camera_proj_view;

    u32 flags                     = 0xFFFF;
    bool freeze_cull_data         = false;
    bool enable_meshlets_pipeline = mesh_shading_supported;

#if TEST_MULTI_OBJECTS
    constexpr u32 kRepeatDraws = 3'375;
    const char* models[]       = {"../data/kitten.obj", "../data/backpack/backpack.obj"};
#else
    constexpr u32 kRepeatDraws = 125'000;
    const char* models[]       = {"../data/kitten.obj"};
#endif

    u64 scene_triangles_max = populate_scene(kRepeatDraws, models, COUNT_OF(models), client_scene, geometry_pool);
    upload_draw_data(geometry_pool.transfer, meshes_transforms, meshes_data, client_scene);

    auto get_time = []<typename T = f64>()
    {
        return static_cast<T>(SDL_GetPerformanceCounter()) / static_cast<T>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    camera_controller controller(client_events, camera);

    render::debug::frustum_renderer frustum_renderer(renderer);

    auto render_loop = [&]()
    {
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update(camera_transform, camera_data, static_cast<f32>(dt));

        if (!renderer.acquire_frame())
        {
            return;
        }

        renderer.submit(
            [&](VkCommandBuffer buffer)
            {
                ZoneScopedN("main.renderer.submit");
                constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};

                const auto viewport = renderer.get_viewport();
                const auto scissor  = renderer.get_scissor();

                vkBeginCommandBuffer(buffer, &command_buffer_begin_info);
                TRACY_ONLY(TracyVkCollect(renderer.get_frame_tracy_context(), buffer));

                vkCmdResetQueryPool(buffer, timestamp_query_pool, 0, kQueryPoolCount);
                if (pipeline_statistics_query)
                {
                    vkCmdResetQueryPool(buffer, pipeline_statistics_query, 0, kQueryPoolCount);
                }

                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, 0);

                if (!freeze_cull_data)
                {
                    auto projection = client_render_settings.render_distance > 0
                                        ? camera_data.get_projection_matrix(client_render_settings.render_distance)
                                        : camera_data.get_projection_matrix();

                    auto view = camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);

                    (*static_cast<frame_cull_data*>(frame_cull_data_buffer.mapped)) =
                        frame_cull_data {
                            .pyramid_size = depth_pyramid.base_size, .draw_count = kRepeatDraws, .flags = flags}
                            .build_frustum(projection, view);
                }
                else
                {
                    static_cast<frame_cull_data*>(frame_cull_data_buffer.mapped)->flags = flags;
                }

                camera_proj_view = camera_data.get_projection_matrix()
                                 * camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "cull last frame occluders"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {meshes_data.buffer,
                                                                             meshes_transforms.buffer,
                                                                             draw_count_buffer.buffer,
                                                                             mesh_visibility_buffer.buffer,
                                                                             frame_cull_data_buffer.buffer,
                                                                             enable_meshlets_pipeline
                                                                                 ? meshlets_draw_indirect_buffer.buffer
                                                                                 : indexed_draw_indirect_buffer.buffer};

                    const render::vk_pipeline& cull_pass =
                        enable_meshlets_pipeline ? meshlets_cull_pipeline : indexed_cull_pipeline;
                    cull_pass.bind(buffer);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, kRepeatDraws, 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL);

                render::transition_image(buffer,
                                         depth_image.image.image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_ASPECT_DEPTH_BIT);
                begin_rendering(buffer,
                                renderer.get_frame_swapchain_image().image_view,
                                depth_image.image.view,
                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                VK_ATTACHMENT_STORE_OP_STORE,
                                renderer.get_scissor());

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                const auto& render_pipeline =
                    enable_meshlets_pipeline ? meshlets_render_pipeline : indexed_render_pipeline;
                render_pipeline.bind(buffer);
                render_pipeline.push_constant(buffer, pc_data {.pv = camera_proj_view});

                if (pipeline_statistics_query)
                {
                    vkCmdBeginQuery(buffer, pipeline_statistics_query, 0, 0);
                }

                {
                    ZoneScopedN("draw last frame occluders");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "draw last frame occluders"));

                    draw_scene(enable_meshlets_pipeline,
                               buffer,
                               render_pipeline,
                               geometry_pool,
                               meshes_data,
                               meshes_transforms,
                               draw_count_buffer,
                               enable_meshlets_pipeline ? meshlets_draw_indirect_buffer : indexed_draw_indirect_buffer,
                               frame_cull_data_buffer,
                               kRepeatDraws);
                }

                if (pipeline_statistics_query)
                {
                    vkCmdEndQuery(buffer, pipeline_statistics_query, 0);
                }
                vkCmdEndRendering(buffer);

                // Reduce the depth buffer pyramid
                if (!freeze_cull_data)
                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "depth reduce"));

                    render::transition_image(
                        buffer, depth_pyramid.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

                    depth_reduce_pipeline.bind(buffer);
                    for (i32 i = 0; i < depth_pyramid.pyramid_count; ++i)
                    {
                        const render::vk_descriptor_info cull_pass_bindings[] = {
                            render::vk_descriptor_info(depth_pyramid.sampler,
                                                       i == 0 ? depth_image.image.view : depth_pyramid.views[i - 1],
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

                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "cull last frame occluders"));

                    reset_draw_count_buffer(buffer, draw_count_buffer);
                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        meshes_data.buffer,
                        meshes_transforms.buffer,
                        draw_count_buffer.buffer,
                        mesh_visibility_buffer.buffer,
                        frame_cull_data_buffer.buffer,
                        enable_meshlets_pipeline ? meshlets_draw_indirect_buffer.buffer
                                                 : indexed_draw_indirect_buffer.buffer,
                        render::vk_descriptor_info(
                            depth_pyramid.sampler, depth_pyramid.image.view, VK_IMAGE_LAYOUT_GENERAL)};

                    const render::vk_pipeline& cull_pass =
                        enable_meshlets_pipeline ? meshlets_occlusion_cull_pipeline : indexed_cull_occlusion_pipeline;
                    cull_pass.bind(buffer);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);

                    cull_pass.dispatch(buffer, kRepeatDraws, 1, 1);

                    render::cmd_stage_barrier(
                        buffer,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                        VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
                }

                {
                    ZoneScopedN("draw last frame occluders");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "draw new objects"));

                    begin_rendering(buffer,
                                    renderer.get_frame_swapchain_image().image_view,
                                    depth_image.image.view,
                                    VK_ATTACHMENT_LOAD_OP_LOAD,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    renderer.get_scissor());
                    draw_scene(enable_meshlets_pipeline,
                               buffer,
                               render_pipeline,
                               geometry_pool,
                               meshes_data,
                               meshes_transforms,
                               draw_count_buffer,
                               enable_meshlets_pipeline ? meshlets_draw_indirect_buffer : indexed_draw_indirect_buffer,
                               frame_cull_data_buffer,
                               kRepeatDraws);

                    if (freeze_cull_data)
                    {
                        frustum_renderer.draw(buffer, camera_proj_view, frame_cull_data_buffer);
                    }

                    vkCmdEndRendering(buffer);
                }

#if !NO_EDITOR
                {
                    ZoneScopedN("main.draw.editor");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "editor"));

                    editor.begin_frame(renderer);

                    ImGui::SeparatorText("camera controller");
                    codegen::draw(controller);

                    if (ImGui::CollapsingHeader("Camera data", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        codegen::draw(camera_data);
                        codegen::draw(camera_transform);
                    }

                    ImGui::SeparatorText("renderer settings");
                    codegen::draw(client_render_settings);
                    ImGui::BeginDisabled(!mesh_shading_supported);
                    ImGui::Checkbox("Enable meshlets path", &enable_meshlets_pipeline);
                    ImGui::EndDisabled();

                    ImGui::SeparatorText("gpu timings");
                    codegen::draw(profile_data);
                    ImGui::Text("Total triangless rendered:");
                    ImGui::ProgressBar(profile_data.tris_from_max);
                    ImGui::Text("Tris Max: %s", format_big_number(profile_data.tris_in_scene_max).c_str());
                    ImGui::Text("Tris Drawn: %s", format_big_number(profile_data.tris_in_scene_total).c_str());

                    ImGui::SeparatorText("gpu stats");
                    draw_shared_buffer_stats("Vertices", geometry_pool.vertex);
                    draw_shared_buffer_stats("Indices", geometry_pool.index);
                    draw_shared_buffer_stats("Meshlets", geometry_pool.meshlets);
                    draw_shared_buffer_stats("Meshlets payload", geometry_pool.meshlets_payload);

                    ImGui::SeparatorText("render controls");

                    const char* names[] = {
                        "Dummy",
                        "LODs",
                        "Frustum cull",
                        "Occlusion cull",
                        "Meshlets cone cull",
                        "Meshlets frustum cull",
                        "Meshlets occlusion cull",
                    };
                    ImGuiEx::Bits(flags, names, COUNT_OF(names));

                    if ((freeze_cull_data && ImGui::Button("Unfreeze cull data"))
                        || (!freeze_cull_data && ImGui::Button("Freeze cull data")))
                    {
                        freeze_cull_data = !freeze_cull_data;
                    }

                    if (ImGui::CollapsingHeader("Render targets", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        static int img_in_line = 2;
                        ImGui::SliderInt("Images in line", &img_in_line, 1, 2);

                        const auto size_x = ImGui::GetContentRegionAvail().x / static_cast<f32>(img_in_line);
                        const auto size_y = (ImGui::GetContentRegionAvail().x / static_cast<f32>(img_in_line))
                                          / camera_data.aspect_ratio;

                        editor.depth_image(depth_image.image.image,
                                           depth_image.image.view,
                                           VK_IMAGE_LAYOUT_GENERAL,
                                           {0, 1, 1, 0},
                                           {size_x, size_y});
                        if (img_in_line > 1)
                            ImGui::SameLine();

                        editor.image(renderer.get_frame_swapchain_image().image,
                                     renderer.get_frame_swapchain_image().image_view,
                                     VK_IMAGE_LAYOUT_GENERAL,
                                     {0, 1, 1, 0},
                                     {size_x, size_y});
                    }

                    if (ImGui::CollapsingHeader("Depth pyramid", ImGuiTreeNodeFlags_DefaultOpen))
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
                                     {size_x, size_y});
                    }

                    editor.end_frame(renderer);
                }
#endif

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, 1);

                vkEndCommandBuffer(buffer);
                renderer.present_frame(buffer);

#if !NO_PERF_QUERY
                u64 query_results[2];
                vkDeviceWaitIdle(renderer.get_context().device);
                VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(renderer.get_context().device,
                                                        timestamp_query_pool,
                                                        0,
                                                        COUNT_OF(query_results),
                                                        sizeof(query_results),
                                                        query_results,
                                                        sizeof(query_results[0]),
                                                        VK_QUERY_RESULT_64_BIT));

                u64 scene_triangles = 0;
                if (pipeline_statistics_query)
                {
                    VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(renderer.get_context().device,
                                                            pipeline_statistics_query,
                                                            0,
                                                            1,
                                                            sizeof(scene_triangles),
                                                            &scene_triangles,
                                                            sizeof(scene_triangles),
                                                            VK_QUERY_RESULT_64_BIT));
                }

                VkPhysicalDeviceProperties props = {};
                vkGetPhysicalDeviceProperties(renderer.get_context().physical_device, &props);

                profile_data.update(static_cast<f64>(query_results[0]) * props.limits.timestampPeriod * 1e-6,
                                    static_cast<f64>(query_results[1]) * props.limits.timestampPeriod * 1e-6,
                                    scene_triangles,
                                    scene_triangles_max);

                const auto str = cpp::stack_string::make_formatted("CPU: %.3lfms; GPU: %.3lfms; Tris/s (B): %lf",
                                                                   dt * 1000.0F,
                                                                   profile_data.gpu_render_time,
                                                                   profile_data.tris_per_second);
                SDL_SetWindowTitle(client_window.get_native_handle().window, str.c_str());
#endif

                FrameMark;
            });
    };

    std::function wrapper(render_loop);
    client_events.add_watcher(
        event_type::request_draw,
        [](auto&, void* user_data)
        {
            std::invoke(*static_cast<std::function<void()>*>(user_data));
        },
        &wrapper);

    while (!exit)
    {
        client_events.poll();
    }

    return 0;
}
