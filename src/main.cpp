#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

#include <codegen/imgui/gpu_profile_data.hpp>
#include <codegen/scene/components.hpp>
#include <events_queue.hpp>
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

struct pc_data
{
    glm::mat4 pv;
    vec4 sun_pos;
    vec4 camera_pos;
    vec4 camera_view;
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
                              .enable(render::rendering_features_table::eValidation)
#if SM_USE_MESHLETS
                              .enable(render::rendering_features_table::eMeshShading)
#endif
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
    client_events.set_watcher(event_type::request_close,
                              [&](auto&)
                              {
                                  exit = true;
                              });
    client_events.set_watcher(event_type::window_size_changed,
                              [&](auto&)
                              {
                                  renderer.resize_swapchain(client_window.get_size_in_px());
                              });

    const VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset     = 0,
        .size       = sizeof(pc_data),
    };

    render::vk_shader shaders[] = {
#if SM_USE_MESHLETS
#if SM_USE_TS
        *render::vk_shader::load(renderer, "../shaders/meshlets.task.spv"),
#endif
        *render::vk_shader::load(renderer, "../shaders/meshlets.mesh.spv"),
#else
        *render::vk_shader::load(renderer, "../shaders/mesh.vert.spv"),
#endif
        *render::vk_shader::load(renderer, "../shaders/meshlets.frag.spv"),
    };

    const auto render_pipeline = *render::vk_pipeline::create_graphics(renderer, shaders, COUNT_OF(shaders), &range, 1);

    imgui_layer editor(client_window, renderer);

    // test scene stuff
    scene client_scene;
    auto camera = client_scene.create_entity();
    auto sun    = client_scene.create_entity();

    sun.add_component<id_component>(DEBUG_ONLY(id_component("sun")));
    sun.add_component<transform_component>();
    sun.add_component<directional_light_component>(
        directional_light_component {.color = vec3(1.0F, 1.0F, 1.0F), .direction = vec3(0.5)});

    camera.add_component<id_component>(DEBUG_ONLY(id_component("camera")));
    camera.add_component<transform_component>(transform_component {.position = vec3(0, 0, 5)});
    camera.add_component<camera_component>(camera_component {
        .far_plane      = 1000.0F,
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(90.0F),
    });

    render::vk_scene_geometry_pool geometry_pool {
#if !SM_USE_MESHLETS
        .index = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
#endif
        .vertex = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
#if SM_USE_MESHLETS
        .meshlets = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
#endif
        .transfer = *render::create_buffer_transfer(renderer.get_context().device,
                                                    renderer.get_context().allocator,
                                                    renderer.get_context().queues[render::queue_kind::eTransfer],
                                                    128 * 1024 * 1024)};

#define KITTY 0

    // FIXME: multiple object support
#if KITTY  // It was broken quite a while actually, ever since I moved vertices into SSBO
    auto kitten = client_scene.create_entity();
    kitten.add_component<id_component>(DEBUG_ONLY(id_component("kitten model")));
    kitten.add_component<transform_component>();
    kitten.add_component<static_model_component>(
        *static_model::load_model("../data/kitten.obj", renderer, geometry_pool));
#endif

#if !KITTY
    auto backpack = client_scene.create_entity();
    backpack.add_component<id_component>(DEBUG_ONLY(id_component("backpack model")));
    backpack.add_component<transform_component>();
    backpack.add_component<static_model_component>(
        *static_model::load_model("../data/backpack/backpack.obj", renderer, geometry_pool));
#endif

    constexpr u32 kQueryPoolCount    = 64;
    VkQueryPool timestamp_query_pool = *render::create_vk_query_pool(renderer.get_context().device, kQueryPoolCount);

    gpu_profile_data profile_data;
    vec4 cull_camera_dir;
    bool freeze_camera_cull_dir = false;

    auto render_loop = [&](auto&)
    {
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
                                           .depthStencil = {1.0F, 0},
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

                auto& camera_transform = camera.get_component<transform_component>();
                auto& camera_data      = camera.get_component<camera_component>();

                if (!freeze_camera_cull_dir)
                {
                    cull_camera_dir = vec4(camera_data.get_direction(camera_transform.rotation), 0.0F);
                }

                render_pipeline.bind(buffer);
                render_pipeline.push_constant(
                    buffer,
                    pc_data {.pv = camera_data.get_projection_matrix()
                                 * camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation),
                             .sun_pos     = vec4(sun.get_component<directional_light_component>().direction, 0.0F),
                             .camera_pos  = vec4(camera_transform.position, 1.0F),
                             .camera_view = cull_camera_dir});

#if SM_USE_MESHLETS
                const render::vk_descriptor_info updates[] = {geometry_pool.vertex.buffer.buffer,
                                                              geometry_pool.meshlets.buffer.buffer};
                render_pipeline.push_descriptor_set(buffer, updates);
#else
                const render::vk_descriptor_info updates[] = {geometry_pool.vertex.buffer.buffer};
                render_pipeline.push_descriptor_set(buffer, updates);
#endif

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

#if !SM_USE_MESHLETS
                vkCmdBindIndexBuffer(buffer, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
#endif

                u32 scene_triangles        = 0;
                u32 scene_meshlets         = 0;
                constexpr u32 kRepeatDraws = 100;

                client_scene.get_view<static_model_component>().each(
                    [&](static_model_component& component)
                    {
                        ZoneScopedN("main.component.model.draw");
                        TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "draw call"));

                        scene_meshlets += component.model.meshlets_count();
                        scene_triangles += component.model.indices_count() / 3;
                        for (u32 i = 0; i < kRepeatDraws; ++i)
                        {
                            component.model.draw(buffer);
                        }
                    });

#if !NO_EDITOR
                {
                    ZoneScopedN("main.draw.editor");
                    TRACY_ONLY(TracyVkZone(renderer.get_frame_tracy_context(), buffer, "editor"));

                    editor.begin_frame();
                    ImGui::SeparatorText("gpu timings");
                    codegen::draw(profile_data);
                    draw_tris_per_meshlet_score(profile_data.tris_per_meshlet);

                    ImGui::SeparatorText("gpu stats");
                    draw_shared_buffer_stats("Vertices", geometry_pool.vertex);
#if SM_USE_MESHLETS
                    draw_shared_buffer_stats("Meshlets", geometry_pool.meshlets);
#else
                    draw_shared_buffer_stats("Indices", geometry_pool.index);
#endif

                    ImGui::SeparatorText("render controls");
                    ImGui::Checkbox("Freeze culling data", &freeze_camera_cull_dir);
                    ImGui::Text(
                        "Cull direction: [%f, %f, %f]", cull_camera_dir.x, cull_camera_dir.y, cull_camera_dir.z);

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
                vkDeviceWaitIdle(renderer.get_context().device);

#if !defined(NDEBUG) || 1
                uint64_t query_results[2];
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

                auto str = cpp::stack_string::make_formatted(
                    "GPU time: %lf; Tris/s (B): %lf", profile_data.gpu_render_time, profile_data.tris_per_second);
                SDL_SetWindowTitle(client_window.get_native_handle().window, str.c_str());
#endif

                FrameMark;
            });
    };

    client_events.set_watcher(event_type::request_draw, render_loop);

    while (!exit)
    {
        client_events.poll();
    }

    return 0;
}
