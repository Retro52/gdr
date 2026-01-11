#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

#include <camera_controller.hpp>
#include <codegen/camera_controller.hpp>
#include <codegen/imgui/gpu_profile_data.hpp>
#include <codegen/scene/components.hpp>
#include <events.hpp>
#include <fs/fs.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui/gpu_profile_data.hpp>
#include <imgui/imgui_layer.hpp>
#include <imgui/imgui_utils.hpp>
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

// FIXME: proper clean-up for vk handles

struct alignas(16) mesh_draw_command
{
    vec4 pos_and_scale;
    glm::quat rotation_quat;

    union
    {
        VkDrawIndexedIndirectCommand indexed;
        VkDrawMeshTasksIndirectCommandEXT mesh;
    };
};

struct pc_data
{
    glm::mat4 pv;
    glm::mat4 view;
    u32 use_culling {1};
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
    const f64 upper_bound = static_cast<f64>(static_model::kMaxTrianglesPerMeshlet);

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

int main(int argc, char* argv[])
{
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
        event_type::window_size_changed,
        +[](const event_payload& payload, void* user_data)
        {
            auto& renderer = *static_cast<render::vk_renderer*>(user_data);
            renderer.resize_swapchain(payload.window.size_px);
        },
        &renderer);

    const VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset     = 0,
        .size       = sizeof(pc_data),
    };

    render::vk_shader shaders[] = {
#if SM_USE_MESHLETS
        *render::vk_shader::load(renderer, "../shaders/meshlets.task.spv"),
        *render::vk_shader::load(renderer, "../shaders/meshlets.mesh.spv"),
#else
        *render::vk_shader::load(renderer, "../shaders/mesh.vert.spv"),
#endif
        *render::vk_shader::load(renderer, "../shaders/meshlets.frag.spv"),
    };

    const auto render_pipeline = *render::vk_pipeline::create_graphics(renderer, shaders, COUNT_OF(shaders), &range, 1);

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

#define KITTY 1

    // FIXME: multiple object support
#if KITTY  // It was broken quite a while actually, ever since I moved vertices into SSBO
    auto kitten = client_scene.create_entity();
    kitten.add_component<id_component>(DEBUG_ONLY(id_component("kitten model")));
    kitten.add_component<transform_component>();
    kitten.add_component<static_model_component>(
        *static_model::load_model("../data/kitten.obj", renderer, geometry_pool));
#endif

#if !KITTY
    auto random_guy = client_scene.create_entity();
    random_guy.add_component<id_component>(DEBUG_ONLY(id_component("random guy model")));
    random_guy.add_component<transform_component>();
    random_guy.add_component<static_model_component>(
        *static_model::load_model("../data/guy.obj", renderer, geometry_pool));
#endif

    constexpr u32 kQueryPoolCount    = 64;
    VkQueryPool timestamp_query_pool = *render::create_vk_query_pool(renderer.get_context().device, kQueryPoolCount);

    gpu_profile_data profile_data;

    glm::mat4 camera_proj_view;
    glm::mat4 camera_cull_view;
    bool enable_cone_culling    = true;
    bool freeze_camera_cull_dir = false;

    constexpr u32 kRepeatDraws    = 3'375;
    const u32 kVolumeItemsPerSide = std::lround(std::cbrt(kRepeatDraws));

    render::vk_buffer draw_indirect_buffer = *render::create_buffer(
        kRepeatDraws * sizeof(mesh_draw_command),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        renderer.get_context().allocator,
        0);

    auto get_time = []()
    {
        return static_cast<f64>(SDL_GetPerformanceCounter()) / static_cast<f64>(SDL_GetPerformanceFrequency());
    };

    f64 last_frame_time = get_time();
    camera_controller controller(client_events, camera);

    auto render_loop = [&]()
    {
        const f64 current_time = get_time();
        const f64 dt           = current_time - last_frame_time;

        last_frame_time = current_time;

        auto& camera_transform = camera.get_component<transform_component>();
        auto& camera_data      = camera.get_component<camera_component>();
        controller.update_position(camera_transform, camera_data, dt);

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

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL);

                vkCmdBeginRendering(buffer, &rendering_info);

                camera_proj_view = camera_data.get_projection_matrix()
                                 * camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);

                if (!freeze_camera_cull_dir)
                {
                    camera_cull_view =
                        camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation);
                }

                render_pipeline.bind(buffer);

#if SM_USE_MESHLETS
                const render::vk_descriptor_info updates[] = {
                    geometry_pool.vertex.buffer.buffer,
                    geometry_pool.meshlets.buffer.buffer,
                    geometry_pool.meshlets_payload.buffer.buffer,
                    draw_indirect_buffer.buffer,
                };
                render_pipeline.push_descriptor_set(buffer, updates);
#else
                const render::vk_descriptor_info updates[] = {geometry_pool.vertex.buffer.buffer,
                                                              draw_indirect_buffer.buffer};
                render_pipeline.push_descriptor_set(buffer, updates);
#endif

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

#if !SM_USE_MESHLETS
                vkCmdBindIndexBuffer(buffer, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
#endif

                u32 scene_triangles = 0;
                u32 scene_meshlets  = 0;

                client_scene.get_view<static_model_component>().each(
                    [&](static_model_component& component)
                    {
                        ZoneScopedN("main.component.model.draw");
                        TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "draw call"));

                        scene_meshlets += component.model.meshlets_count();
                        scene_triangles += component.model.indices_count() / 3;

                        {
                            ZoneScopedN("FIXME.data.upload");
                            std::array<mesh_draw_command, kRepeatDraws> draw_cmds {};

                            for (u32 i = 0; i < kRepeatDraws; ++i)
                            {
                                draw_cmds[i].pos_and_scale = {i % kVolumeItemsPerSide,
                                                              (i / kVolumeItemsPerSide) % kVolumeItemsPerSide,
                                                              i / (kVolumeItemsPerSide * kVolumeItemsPerSide),
                                                              1.0F};
#if !KITTY
                                draw_cmds[i].pos_and_scale *= vec4(20.0F, 20.0F, 20.0F, 1.0F);
#else
                                draw_cmds[i].pos_and_scale *= vec4(1.5F, 1.5F, 1.5F, 1.0F);
#endif
                                draw_cmds[i].rotation_quat = glm::quatLookAt(
                                    glm::normalize(vec3(draw_cmds[i].pos_and_scale) - camera_transform.position),
                                    vec3(0, 1, 0));
#if SM_USE_MESHLETS
                                draw_cmds[i].mesh.groupCountX =
                                    component.model.meshlets_count() / shader_constants::kTaskWorkGroups;
                                draw_cmds[i].mesh.groupCountY = 1;
                                draw_cmds[i].mesh.groupCountZ = 1;
#else
                                draw_cmds[i].indexed.firstIndex    = 0;
                                draw_cmds[i].indexed.firstInstance = 0;
                                draw_cmds[i].indexed.vertexOffset  = 0;
                                draw_cmds[i].indexed.instanceCount = 1;
                                draw_cmds[i].indexed.indexCount    = component.model.indices_count();
#endif
                            }
                            render::upload_data(geometry_pool.transfer,
                                                draw_indirect_buffer,
                                                reinterpret_cast<u8*>(draw_cmds.data()),
                                                {.size = sizeof(mesh_draw_command) * draw_cmds.size()});
                        }

                        render_pipeline.push_constant(buffer,
                                                      pc_data {.pv          = camera_proj_view,
                                                               .view        = camera_cull_view,
                                                               .use_culling = enable_cone_culling});

#if SM_USE_MESHLETS
                        vkCmdDrawMeshTasksIndirectEXT(buffer,
                                                      draw_indirect_buffer.buffer,
                                                      offsetof(mesh_draw_command, mesh),
                                                      kRepeatDraws,
                                                      sizeof(mesh_draw_command));
#else
                        vkCmdDrawIndexedIndirect(buffer,
                                                 draw_indirect_buffer.buffer,
                                                 offsetof(mesh_draw_command, indexed),
                                                 kRepeatDraws,
                                                 sizeof(mesh_draw_command));
#endif
                    });

#if !NO_EDITOR
                {
                    ZoneScopedN("main.draw.editor");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "editor"));

                    editor.begin_frame();

                    ImGui::SeparatorText("camera controller");
                    codegen::draw(controller);

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

                auto str = cpp::stack_string::make_formatted("CPU: %.3lfms; GPU: %.3lfms; Tris/s (B): %lf",
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
