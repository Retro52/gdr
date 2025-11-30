#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <types.hpp>

#include <events_queue.hpp>
#include <fs.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui/imgui_layer.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <render/platform/vk/vk_vertex_desc.hpp>
#include <scene/components.hpp>
#include <window.hpp>

#include <vector>

struct pc_data
{
    glm::mat4 pv;
    vec4 sun_pos;
    vec4 camera_pos;
};

int SDL_main(int argc, char* argv[])
{
    window client_window("VK window", {1920, 960}, false);
    events_queue client_events(client_window);

    auto features_table = render::rendering_features_table()
                              .enable(render::rendering_features_table::eValidation)
                              .enable(render::rendering_features_table::eDynamicRender)
                              .enable(render::rendering_features_table::eSynchronization2);

    render::vk_renderer renderer(
        render::instance_desc {
            .app_name          = "DYE",
            .app_version       = 1,
            .device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME},
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

    const auto vert_shader_module = renderer.create_shader_module(read_file("../shaders/bf.vert.spv"));
    const auto frag_shader_module = renderer.create_shader_module(read_file("../shaders/bf.frag.spv"));

    const VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset     = 0,
        .size       = sizeof(pc_data),
    };

    u32 vertex_desc_count  = 0;
    const auto vertex_bind = render::get_vertex_binding<static_model::static_model_vertex>();
    const auto vertex_desc = render::get_vertex_description<static_model::static_model_vertex>(&vertex_desc_count);

    const auto render_pipeline = *render::pipeline::create_graphics(
        renderer, vert_shader_module, frag_shader_module, &range, 1, vertex_bind, vertex_desc, vertex_desc_count);

    renderer.destroy_shader_module(vert_shader_module);
    renderer.destroy_shader_module(frag_shader_module);

    imgui_layer editor(client_window, renderer);

    static_model model = *static_model::load_model(renderer, read_file("../data/kitten.obj"));
    // static_model model = *static_model::load_model(renderer, read_file("../data/backpack/backpack.obj"));

    constexpr camera_component camera {
        .far_plane      = 1000.0F,
        .near_plane     = 0.01F,
        .aspect_ratio   = 16.0F / 9.0F,
        .horizontal_fov = glm::radians(90.0F),
    };

    vec3 sun_pos         = {0.5, 0.5, -0.5};
    vec3 camera_pos      = {0, 0, 5};
    const glm::quat kRot = vec3 {glm::radians(0.0F), glm::radians(-5.0F), glm::radians(0.0F)};

    auto render_loop = [&](auto&)
    {
        if (!renderer.acquire_frame())
        {
            return;
        }

        renderer.submit(
            [&](VkCommandBuffer buffer)
            {
                const VkCommandBufferBeginInfo command_buffer_begin_info {
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

                render_pipeline.bind(buffer);
                render_pipeline.push_constant(
                    buffer,
                    pc_data {.pv         = camera.get_projection_matrix() * camera.get_view_matrix(camera_pos, kRot),
                             .sun_pos    = vec4(sun_pos, 0.0F),
                             .camera_pos = vec4(camera_pos, 1.0F)});

                vkCmdSetScissor(buffer, 0, 1, &scissor);
                vkCmdSetViewport(buffer, 0, 1, &viewport);

                model.draw(buffer);

                editor.begin_frame();
                ImGui::DragFloat3("Camera pos: ", &camera_pos.x, 0.25F);
                ImGui::DragFloat3("Sun pos: ", &sun_pos.x, 0.25F);
                editor.end_frame(renderer);

                vkCmdEndRendering(buffer);

                render::transition_image(buffer,
                                         renderer.get_frame_swapchain_image().image,
                                         VK_IMAGE_LAYOUT_GENERAL,
                                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vkEndCommandBuffer(buffer);
                renderer.present_frame(buffer);
            });
    };

    client_events.set_watcher(event_type::request_draw, render_loop);

    while (!exit)
    {
        client_events.poll();
    }

    return 0;
}
