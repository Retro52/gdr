#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

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
#include <imgui/imgui_layer.hpp>
#include <imgui/imgui_utils.hpp>
#include <render/debug/frustum_renderer.hpp>
#include <render/platform/vk/vk_barrier.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <render/platform/vk/vk_timestamp.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <Tracy/Tracy.hpp>
#include <window.hpp>

#include <vector>

#define NO_EDITOR 0

struct pc_data
{
    glm::mat4 pv;
    glm::mat4 view;
    u32 use_culling {1};
};

struct cull_data
{
    glm::mat4 view;
    glm::mat4 projection;
};

struct comp_buf_pc
{
    vec4 frustum[6];
    u32 draw_count;

    comp_buf_pc& build_frustum(const glm::mat4& proj, const glm::mat4& view)
    {
        auto t_pv  = glm::transpose(proj * view);
        frustum[0] = t_pv[3] + t_pv[0];  // left
        frustum[1] = t_pv[3] - t_pv[0];  // right
        frustum[2] = t_pv[3] + t_pv[1];  // bottom
        frustum[3] = t_pv[3] - t_pv[1];  // top
        frustum[4] = t_pv[3] - t_pv[2];  // near
        frustum[5] = t_pv[2];

        for (auto& plane : frustum)
        {
            plane /= glm::length(glm::vec3(plane));
        }

        return *this;
    }
};

f64 bytes_to_mb(u64 bytes)
{
    return static_cast<f64>(bytes) / (1024.0 * 1024);
}

void draw_shared_buffer_stats(const char* label, const render::vk_shared_buffer& buffer)
{
    ImGui::TextWrapped("%s buffer: %lf MB used (%lf MB total, %lf%%)",
                       label,
                       bytes_to_mb(buffer.offset),
                       bytes_to_mb(buffer.size),
                       static_cast<f64>(buffer.offset) * 100.0 / buffer.size);
}

void draw_tris_per_meshlet_score(f64 tpm_average)
{
#if SM_USE_MESHLETS
    constexpr f64 upper_bound = static_cast<f64>(static_model::kMaxTrianglesPerMeshlet);

    if (tpm_average > upper_bound * 0.9)
    {
        ImGuiEx::ScopedColor _(ImGuiCol_Text, IM_COL32(80, 200, 120, 255));
        ImGui::Text("TPM Score: excellent (%lf%% from max)", tpm_average * 100.0F / upper_bound);
    }
    else if (tpm_average > upper_bound * 0.75)
    {
        ImGuiEx::ScopedColor _(ImGuiCol_Text, IM_COL32(50, 200, 120, 255));
        ImGui::Text("TPM Score: good (%lf%% from max)", tpm_average * 100.0F / upper_bound);
    }
    else if (tpm_average > upper_bound * 0.5)
    {
        ImGuiEx::ScopedColor _(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
        ImGui::Text("TPM Score: suboptimal (%lf%% from max)", tpm_average * 100.0F / upper_bound);
    }
    else
    {
        ImGuiEx::ScopedColor _(ImGuiCol_Text, IM_COL32(255, 50, 50, 255));
        ImGui::Text("TPM Score: bad (%lf%% from max)", tpm_average * 100.0F / upper_bound);
    }
#endif
}

f32 get_random_f32(f32 min, f32 max)
{
    return min + (static_cast<f32>(rand()) / RAND_MAX) * (max - min);
}

i32 get_random_i32(i32 min, i32 max)
{
    return min + static_cast<int>(get_random_f32(0.0F, 1.0F) * (max - min));
}

void populate_scene(const u32 draw_count, const char* models[], u32 models_count, scene& scene,
                    render::vk_scene_geometry_pool& geometry_pool)
{
    ZoneScoped;
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
        auto entity = scene.create_entity();
        entity.add_component<static_model_component>(unique_models[get_random_i32(0, models_count)]);
        entity.add_component<id_component>();

        auto& transform = entity.emplace_component<transform_component>();

        transform.position = {
            i % kVolumeItemsPerSide,
            (i / kVolumeItemsPerSide) % kVolumeItemsPerSide,
            i / (kVolumeItemsPerSide * kVolumeItemsPerSide),
        };

        transform.position *=
            vec3(get_random_f32(-20.0F, 20.0F), get_random_f32(-20.0F, 20.0F), get_random_f32(-20.0F, 20.0F));
        transform.uniform_scale = get_random_f32(0.1F, 2.25F);
        transform.rotation      = glm::quat(vec3(get_random_f32(0, 89), get_random_f32(0, 89), get_random_f32(0, 89)));
    }
}

void upload_draw_data(const render::vk_buffer_transfer& transfer, const render::vk_buffer& transform_buffer,
                      const render::vk_buffer& mesh_data_buffer, const scene& scene)
{
    ZoneScoped;

    auto&& view              = scene.get_view<transform_component, static_model_component>();
    const u64 view_size_hint = view.size_hint();

    transform_component* transforms = static_cast<transform_component*>(transfer.mapped);
    static_model* static_models     = reinterpret_cast<static_model*>(static_cast<u8*>(transfer.mapped)
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
    events_queue client_events(client_window);

    auto features_table = render::rendering_features_table()
#if !defined(NDEBUG)
                              .enable(render::rendering_features_table::eValidation)
#endif
#if SM_USE_MESHLETS
                              .enable(render::rendering_features_table::eMeshShading)
#endif
                              .enable(render::rendering_features_table::eDrawIndirect)
                              .enable(render::rendering_features_table::eDynamicRender)
                              .enable(render::rendering_features_table::eSynchronization2);

    render::vk_renderer renderer(
        render::instance_desc {
            .device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if SM_USE_MESHLETS
                                  VK_EXT_MESH_SHADER_EXTENSION_NAME,
#endif
                                  VK_KHR_MAINTENANCE_4_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
                                  VK_KHR_8BIT_STORAGE_EXTENSION_NAME, TRACY_ONLY(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)},
            .app_name        = "Vulkan renderer",
            .app_version     = 1,
            .device_features = features_table,
    },
        client_window);

    bool exit = false;
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

    client_events.add_watcher(
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& renderer = *static_cast<render::vk_renderer*>(user_data);
            renderer.resize_swapchain(payload.window.size_px);
        },
        &renderer);

    render::vk_shader shaders[] = {
#if SM_USE_MESHLETS
        *render::vk_shader::load(renderer, "../shaders/meshlets.task.spv"),
        *render::vk_shader::load(renderer, "../shaders/meshlets.mesh.spv"),
#else
        *render::vk_shader::load(renderer, "../shaders/mesh.vert.spv"),
#endif
        *render::vk_shader::load(renderer, "../shaders/meshlets.frag.spv"),
    };

    const auto render_pipeline = *render::vk_pipeline::create_graphics(renderer, shaders, COUNT_OF(shaders));
    const auto cull_pass =
        *render::vk_pipeline::create_compute(renderer,
#if SM_USE_MESHLETS
                                             *render::vk_shader::load(renderer, "../shaders/meshlets_cull.comp.spv")
#else
                                             *render::vk_shader::load(renderer, "../shaders/mesh_cull.comp.spv")
#endif
        );

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
    camera.add_component<transform_component>(transform_component {.position = vec3(5, 5, 25)});
    camera.add_component<camera_component>(camera_component {
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(90.0F),
    });

    render::vk_scene_geometry_pool geometry_pool {
#if !SM_USE_MESHLETS
        .index = render::vk_shared_buffer(renderer, 32 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
#endif
        .vertex = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
#if SM_USE_MESHLETS
        .meshlets         = render::vk_shared_buffer(renderer, 8 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .meshlets_payload = render::vk_shared_buffer(renderer, 16 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
#endif
        .transfer = *render::create_buffer_transfer(renderer.get_context().device,
                                                    renderer.get_context().allocator,
                                                    renderer.get_context().queues[render::queue_kind::eTransfer],
                                                    64 * 1024 * 1024)};

    constexpr u32 kQueryPoolCount    = 64;
    VkQueryPool timestamp_query_pool = *render::create_vk_query_pool(renderer.get_context().device, kQueryPoolCount);

    render::vk_buffer meshes_data =
        *render::create_buffer(4 * 1024 * 1024,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               renderer.get_context().allocator,
                               0);

    render::vk_buffer meshes_transforms =
        *render::create_buffer(4 * 1024 * 1024,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               renderer.get_context().allocator,
                               0);

    render::vk_buffer draw_indirect_buffer = *render::create_buffer(
        1024 * 1024,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    gpu_profile_data profile_data;

    cull_data cull_matrices {};
    render_settings client_render_settings;

    glm::mat4 camera_proj_view;
    bool enable_cone_culling    = true;
    bool freeze_camera_cull_dir = false;

    u32 scene_triangles = 0;
    u32 scene_meshlets  = 0;

    constexpr u32 kRepeatDraws = 3'375;
    const char* models[]       = {"../data/kitten.obj", "../data/moon.obj"};

    populate_scene(kRepeatDraws, models, COUNT_OF(models), client_scene, geometry_pool);
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
        controller.update_position(camera_transform, camera_data, static_cast<f32>(dt));

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

                VkRenderingAttachmentInfo color_attachment_info {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView          = renderer.get_frame_swapchain_image().image_view,
                    .imageLayout        = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {
                                           .color = {0.0F, 0.0F, 0.0F, 1.0F},
                                           }
                };

                VkRenderingAttachmentInfo depth_attachment_info {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView          = renderer.get_frame_swapchain_image().depth_image_view,
                    .imageLayout        = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {
                                           .depthStencil = {0.0F, 0},
                                           }
                };

                const VkRenderingInfo rendering_info {.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                                                      .renderArea           = renderer.get_scissor(),
                                                      .layerCount           = 1,
                                                      .colorAttachmentCount = 1,
                                                      .pColorAttachments    = &color_attachment_info,
                                                      .pDepthAttachment     = &depth_attachment_info};

                const auto viewport = renderer.get_viewport();
                const auto scissor  = renderer.get_scissor();

                vkBeginCommandBuffer(buffer, &command_buffer_begin_info);
                TRACY_ONLY(TracyVkCollect(renderer.get_frame_tracy_context(), buffer));

                vkCmdResetQueryPool(buffer, timestamp_query_pool, 0, kQueryPoolCount);
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, 0);

                if (!freeze_camera_cull_dir)
                {
                    cull_matrices.projection =
                        client_render_settings.render_distance > 0
                            ? camera_data.get_projection_matrix(client_render_settings.render_distance)
                            : camera_data.get_projection_matrix();

                    cull_matrices.view =
                        camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);
                }

                if (enable_cone_culling)
                {
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "vk cmd dispatch"));

                    const render::vk_descriptor_info cull_pass_bindings[] = {
                        meshes_data.buffer,
                        meshes_transforms.buffer,
                        draw_indirect_buffer.buffer,
                    };

                    cull_pass.bind(buffer);
                    cull_pass.push_descriptor_set(buffer, cull_pass_bindings);
                    cull_pass.push_constant(buffer,
                                            comp_buf_pc {.draw_count = kRepeatDraws}.build_frustum(
                                                cull_matrices.projection, cull_matrices.view));

                    vkCmdDispatch(buffer, (kRepeatDraws + 31) / 32, 1, 1);

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

                vkCmdBeginRendering(buffer, &rendering_info);

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                render_pipeline.bind(buffer);

#if SM_USE_MESHLETS
                const render::vk_descriptor_info render_bindings[] = {
                    geometry_pool.vertex.buffer.buffer,
                    geometry_pool.meshlets.buffer.buffer,
                    geometry_pool.meshlets_payload.buffer.buffer,
                    meshes_data.buffer,
                    meshes_transforms.buffer,
                };
                render_pipeline.push_descriptor_set(buffer, render_bindings);
#else
                const render::vk_descriptor_info render_bindings[] = {
                    geometry_pool.vertex.buffer.buffer, meshes_data.buffer, meshes_transforms.buffer};
                render_pipeline.push_descriptor_set(buffer, render_bindings);
#endif

#if !SM_USE_MESHLETS
                vkCmdBindIndexBuffer(buffer, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
#endif

                camera_proj_view = camera_data.get_projection_matrix()
                                 * camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);
                {
                    ZoneScopedN("main.component.model.draw");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "draw call"));

                    render_pipeline.push_constant(buffer,
                                                  pc_data {.pv          = camera_proj_view,
                                                           .view        = cull_matrices.view,
                                                           .use_culling = enable_cone_culling});

#if SM_USE_MESHLETS
                    vkCmdDrawMeshTasksIndirectEXT(buffer,
                                                  draw_indirect_buffer.buffer,
                                                  0,
                                                  kRepeatDraws,
                                                  sizeof(VkDrawMeshTasksIndirectCommandEXT));
#else
                    vkCmdDrawIndexedIndirect(
                        buffer, draw_indirect_buffer.buffer, 0, kRepeatDraws, sizeof(VkDrawIndexedIndirectCommand));
#endif
                }
                if (freeze_camera_cull_dir)
                {
                    frustum_renderer.draw(buffer, camera_proj_view, cull_matrices.view, cull_matrices.projection);
                }

#if !NO_EDITOR
                {
                    ZoneScopedN("main.draw.editor");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "editor"));

                    editor.begin_frame();

                    ImGui::SeparatorText("camera controller");
                    codegen::draw(controller);

                    ImGui::SeparatorText("renderer settings");
                    codegen::draw(client_render_settings);

                    ImGui::SeparatorText("gpu timings");
                    codegen::draw(profile_data);
                    draw_tris_per_meshlet_score(profile_data.tris_per_meshlet);

                    ImGui::SeparatorText("gpu stats");
                    draw_shared_buffer_stats("Vertices", geometry_pool.vertex);
#if SM_USE_MESHLETS
                    draw_shared_buffer_stats("Meshlets", geometry_pool.meshlets);
                    draw_shared_buffer_stats("Meshlets payload", geometry_pool.meshlets_payload);
#else
                    draw_shared_buffer_stats("Indices", geometry_pool.index);
#endif

                    ImGui::SeparatorText("render controls");
                    ImGui::Checkbox("Enable culling", &enable_cone_culling);
                    ImGui::Checkbox("Freeze culling data", &freeze_camera_cull_dir);

                    client_scene.get_view<entt::entity>().each(
                        [&](const entt::entity entity)
                        {
                            cpp::stack_string label;

                            if (client_scene.has_component<id_component>(entity))
                            {
                                DEBUG_ONLY(auto& id = client_scene.get_component<id_component>(entity));
                                DEBUG_ONLY(label = cpp::stack_string::make_formatted("Entity '%s'", id.name.c_str()))

                                NDEBUG_ONLY(label = cpp::stack_string::make_formatted("Entity (id: %d)",
                                                                                      static_cast<const int>(entity)));
                            }
                            else
                            {
                                label = cpp::stack_string::make_formatted("Entity (id: %d)",
                                                                          static_cast<const int>(entity));
                            }

                            if (ImGui::CollapsingHeader(label.c_str()))
                            {
                                ImGui::PushID(static_cast<const int>(entity));

                                codegen::detail_components::for_each_type(
                                    [&]<typename component>()
                                    {
                                        if (client_scene.has_component<component>(entity))
                                        {
                                            auto& data = client_scene.get_component<component>(entity);
                                            ImGui::SeparatorText((codegen::type_name<component>).data());
                                            codegen::draw(data);
                                        }
                                    });
                                ImGui::PopID();
                            }
                        });

                    editor.end_frame(renderer);
                }
#endif

                vkCmdEndRendering(buffer);

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestamp_query_pool, 1);

                vkEndCommandBuffer(buffer);
                renderer.present_frame(buffer);

#if !defined(NDEBUG) || 1
                uint64_t query_results[2];
                vkDeviceWaitIdle(renderer.get_context().device);
                VK_ASSERT_ON_FAIL(vkGetQueryPoolResults(renderer.get_context().device,
                                                        timestamp_query_pool,
                                                        0,
                                                        ARRAYSIZE(query_results),
                                                        sizeof(query_results),
                                                        query_results,
                                                        sizeof(query_results[0]),
                                                        VK_QUERY_RESULT_64_BIT));

                VkPhysicalDeviceProperties props = {};
                vkGetPhysicalDeviceProperties(renderer.get_context().physical_device, &props);

                profile_data.update(static_cast<f64>(query_results[0]) * props.limits.timestampPeriod * 1e-6,
                                    static_cast<f64>(query_results[1]) * props.limits.timestampPeriod * 1e-6,
                                    kRepeatDraws * scene_triangles,
                                    kRepeatDraws * scene_meshlets);

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
