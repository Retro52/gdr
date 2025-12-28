#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

#include <codegen/scene/components.hpp>
#include <events_queue.hpp>
#include <fs/fs.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui/imgui_layer.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <scene/components.hpp>
#include <scene/entity.hpp>
#include <scene/scene.hpp>
#include <tracy/Tracy.hpp>
#include <window.hpp>

#include <vector>

struct pc_data
{
    glm::mat4 pv;
    vec4 sun_pos;
    vec4 camera_pos;
};

int main(int argc, char* argv[])
{
    TracySetProgramName("gdr");

    window client_window("VK window", {1920, 960}, false);
    events_queue client_events(client_window);

    auto features_table = render::rendering_features_table()
                              .enable(render::rendering_features_table::eValidation)
                              .enable(render::rendering_features_table::eMeshShading)
                              .enable(render::rendering_features_table::eDynamicRender)
                              .enable(render::rendering_features_table::eSynchronization2);

    render::vk_renderer renderer(
        render::instance_desc {
            .device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                  VK_EXT_MESH_SHADER_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME},
            .app_name          = "Vulkan renderer",
            .app_version       = 1,
            .device_features   = features_table,
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
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(pc_data),
    };

    render::vk_shader shaders[] = {
        // *render::vk_shader::load(renderer, "../shaders/parse_test.comp.spv"),
        *render::vk_shader::load(renderer, "../shaders/mesh.vert.spv"),
        *render::vk_shader::load(renderer, "../shaders/meshlets.frag.spv"),
        // *render::shader::load(renderer, "../shaders/meshlets.task.spv"),
        // *render::shader::load(renderer, "../shaders/meshlets.frag.spv"),
    };

    const auto render_pipeline = *render::vk_pipeline::create_graphics(renderer, shaders, COUNT_OF(shaders), &range, 1);

    imgui_layer editor(client_window, renderer);

    // test scene stuff
    scene client_scene;
    auto kitten   = client_scene.create_entity();
    auto backpack = client_scene.create_entity();
    auto camera   = client_scene.create_entity();
    auto sun      = client_scene.create_entity();

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
        .vertex = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
        .index  = render::vk_shared_buffer(renderer, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
    };

    kitten.add_component<id_component>(DEBUG_ONLY(id_component("kitten model")));
    kitten.add_component<transform_component>();
    kitten.add_component<static_model_component>(
        *static_model::load_model(fs::read_file("../data/kitten.obj"), renderer, geometry_pool));

    backpack.add_component<id_component>(DEBUG_ONLY(id_component("backpack model")));
    backpack.add_component<transform_component>();
    backpack.add_component<static_model_component>(
        *static_model::load_model(fs::read_file("../data/backpack/backpack.obj"), renderer, geometry_pool));

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

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL);

                vkCmdBeginRendering(buffer, &rendering_info);

                auto& camera_transform = camera.get_component<transform_component>();
                auto& camera_data      = camera.get_component<camera_component>();

                render_pipeline.bind(buffer);
                render_pipeline.push_constant(
                    buffer,
                    pc_data {.pv = camera_data.get_projection_matrix()
                                 * camera_data.get_view_matrix(camera_transform.position, camera_transform.rotation),
                             .sun_pos    = vec4(sun.get_component<directional_light_component>().direction, 0.0F),
                             .camera_pos = vec4(camera_transform.position, 1.0F)});

                render::vk_descriptor_info updates[] = {geometry_pool.vertex.buffer.buffer};
                render_pipeline.push_descriptor_set(buffer, updates);

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                vkCmdBindIndexBuffer(buffer, geometry_pool.index.buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                client_scene.get_view<static_model_component>().each(
                    [&buffer](static_model_component& component)
                    {
                        ZoneScoped;
                        component.model.draw(buffer);
                    });

                {
                    ZoneScoped;

                    editor.begin_frame();
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

                                codegen::components::for_each_type(
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

                vkCmdEndRendering(buffer);

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vkEndCommandBuffer(buffer);
                renderer.present_frame(buffer);
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
