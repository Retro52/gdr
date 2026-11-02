#include <janitor.hpp>
#include <render/platform/vk/shaders/vk_imgui_blit_frag_2d.hpp>
#include <render/platform/vk/shaders/vk_imgui_blit_frag_2d_array.hpp>
#include <render/platform/vk/shaders/vk_imgui_blit_vert.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_imgui.hpp>

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <cmath>
#include <imgui.h>

void platform::vk_imgui::begin_frame()
{
    atlas_data.reset_cursor();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void platform::vk_imgui::end_frame(VkImageView rtv, VkCommandBuffer cmd, const uvec4 vp)
{
    VkRenderingAttachmentInfo color_attachment_info {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = rtv,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {
                        .color = {0.0F, 0.0F, 0.0F, 1.0F},
                        }
    };

    const VkRect2D viewport {
        .offset = {.x = static_cast<i32>(vp.x), .y = static_cast<i32>(vp.y)},
        .extent = {.width = vp.z,               .height = vp.w             },
    };

    const VkRenderingInfo rendering_info {.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                                          .renderArea           = viewport,
                                          .layerCount           = 1,
                                          .colorAttachmentCount = 1,
                                          .pColorAttachments    = &color_attachment_info};

    // Purely a debug thingie, will comment out for now as it was pretty stable IIRC
#if 0
    if (ImGui::CollapsingHeader("[imgui_layer] Atlas texture"))
    {
        ImGui::Image(atlas_data.imgui_descriptor, {ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x});
    }
#endif

    ImGui::Render();
    vkCmdBeginRendering(cmd, &rendering_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
    this->flush_pending(cmd);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool platform::vk_imgui::allocate_region(const u32 w, const u32 h, VkOffset2D& out_offset)
{
    auto& atlas = atlas_data;
    if (atlas.cursor.x + w > kAtlasWidth)
    {
        atlas.cursor.y += atlas.cursor.z + kAtlasPadding;
        atlas.cursor.x = kAtlasPadding;
        atlas.cursor.z = 0;
    }

    if (atlas.cursor.y + h > kAtlasHeight)
    {
        return false;
    }

    out_offset.x = static_cast<i32>(atlas.cursor.x);
    out_offset.y = static_cast<i32>(atlas.cursor.y);

    atlas.cursor.x += w + kAtlasPadding;
    atlas.cursor.z = std::max(atlas.cursor.z, h);
    return true;
}

void platform::vk_imgui::image(VkImage image, VkImageView view, const VkImageLayout src_layout, vec4 uv,
                               const ImVec2 size, const f32 mip, const f32 znear)
{
    image_impl(image,
               view,
               size,
               {uv.x, uv.y},
               {uv.z, uv.w},
               src_layout,
               VK_IMAGE_ASPECT_COLOR_BIT,
               sampler_type::sampler2d,
               {znear, mip, 0.0F});
}

void platform::vk_imgui::image_array(VkImage image, VkImageView view, const VkImageLayout src_layout, const f32 layer,
                                     vec4 uv, const ImVec2 size, const f32 mip, const f32 znear)
{
    image_impl(image,
               view,
               size,
               {uv.x, uv.y},
               {uv.z, uv.w},
               src_layout,
               VK_IMAGE_ASPECT_COLOR_BIT,
               sampler_type::sampler2d_array,
               {.znear = znear, .mip_level = mip, .array_layer = layer});
}

void platform::vk_imgui::depth_image(VkImage image, VkImageView view, const VkImageLayout src_layout, vec4 uv,
                                     const ImVec2 size, const f32 mip, const f32 znear)
{
    image_impl(image,
               view,
               size,
               {uv.x, uv.y},
               {uv.z, uv.w},
               src_layout,
               VK_IMAGE_ASPECT_DEPTH_BIT,
               sampler_type::sampler2d,
               {.znear = znear, .mip_level = mip, .array_layer = 0.0F});
}

void platform::vk_imgui::image_impl(VkImage image, VkImageView view, ImVec2 size, ImVec2 uv0, ImVec2 uv1,
                                    const VkImageLayout src_layout, const VkImageAspectFlags aspect,
                                    const sampler_type type, const pc_data& push_constant)
{
    VkOffset2D offset;
    if (!allocate_region(static_cast<u32>(size.x), static_cast<u32>(size.y), offset))
    {
        ImGui::Text("atlas full :(");
        return;
    }

    pending_uploads.push_back({
        .img           = image,
        .view          = view,
        .offset        = offset,
        .extent        = {.width = static_cast<u32>(size.x), .height = static_cast<u32>(size.y)},
        .src_layout    = src_layout,
        .aspect        = aspect,
        .type          = type,
        .push_constant = push_constant
    });

    const ImVec2 atlas_uv0 {static_cast<f32>(offset.x) / kAtlasWidth, static_cast<f32>(offset.y) / kAtlasHeight};
    const ImVec2 atlas_uv1 {(offset.x + size.x) / kAtlasWidth, (offset.y + size.y) / kAtlasHeight};

    uv0.x = std::lerp(atlas_uv0.x, atlas_uv1.x, uv0.x);
    uv0.y = std::lerp(atlas_uv0.y, atlas_uv1.y, uv0.y);
    uv1.x = std::lerp(atlas_uv0.x, atlas_uv1.x, uv1.x);
    uv1.y = std::lerp(atlas_uv0.y, atlas_uv1.y, uv1.y);

    ImGui::Image(atlas_data.imgui_descriptor, size, uv0, uv1);
}

void platform::vk_imgui::flush_pending(const VkCommandBuffer cmd)
{
    if (pending_uploads.empty())
    {
        platform::vk_transition_image(
            cmd, atlas_data.atlas_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return;
    }

    core::heap_array<VkImageMemoryBarrier2> barriers(pending_uploads.size());

    for (u32 i = 0; i < pending_uploads.size(); ++i)
    {
        const auto& req = pending_uploads[i];
        barriers[i]     = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout        = req.src_layout,
                .newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image            = req.img,
                .subresourceRange = {.aspectMask     = req.aspect,
                                     .baseMipLevel   = 0,
                                     .levelCount     = VK_REMAINING_MIP_LEVELS,
                                     .baseArrayLayer = 0,
                                     .layerCount     = VK_REMAINING_ARRAY_LAYERS},
        };
    }

    VkDependencyInfo dep {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<u32>(barriers.size()),
        .pImageMemoryBarriers    = barriers.data(),
    };
    vkCmdPipelineBarrier2(cmd, &dep);

    VkRenderingAttachmentInfo color_attachment {.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                .imageView   = atlas_data.atlas_image.view,
                                                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                                                .clearValue  = {.color = VkClearColorValue()}};

    const VkRenderingInfo rendering_info {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {.offset = {.x = 0, .y = 0}, .extent = {.width = kAtlasWidth, .height = kAtlasHeight}},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
    };

    platform::vk_transition_image(
        cmd, atlas_data.atlas_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdBeginRendering(cmd, &rendering_info);

    for (const auto& req : pending_uploads)
    {
        auto& pipeline = req.type == sampler_type::sampler2d ? blit2d : blit2d_array;

        pipeline.bind(cmd);
        VkViewport viewport {
            .x        = static_cast<f32>(req.offset.x),
            .y        = static_cast<f32>(req.offset.y),
            .width    = static_cast<f32>(req.extent.width),
            .height   = static_cast<f32>(req.extent.height),
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };
        VkRect2D scissor {.offset = req.offset, .extent = req.extent};

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        pipeline.push_constant(cmd, req.push_constant);

        const platform::vk_descriptor_info bindings[] {
            {atlas_data.sampler, req.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
        };

        pipeline.push_descriptor_set(cmd, bindings);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    // Back to original
    for (u32 i = 0; i < pending_uploads.size(); ++i)
    {
        const auto& req = pending_uploads[i];
        barriers[i]     = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .oldLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout        = req.src_layout,
                .image            = req.img,
                .subresourceRange = {.aspectMask     = req.aspect,
                                     .baseMipLevel   = 0,
                                     .levelCount     = VK_REMAINING_MIP_LEVELS,
                                     .baseArrayLayer = 0,
                                     .layerCount     = VK_REMAINING_ARRAY_LAYERS},
        };
    }

    dep.pImageMemoryBarriers    = barriers.data();
    dep.imageMemoryBarrierCount = static_cast<u32>(barriers.size());
    vkCmdPipelineBarrier2(cmd, &dep);

    pending_uploads.clear();
    platform::vk_transition_image(cmd,
                                  atlas_data.atlas_image.image,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

auto platform::vk_imgui_create_context(const platform::vk_context& ctx, SDL_Window* window, VkFormat format,
                                       u32 sc_image_count) -> result<vk_imgui>
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGuiStyle& style   = ImGui::GetStyle();
    style.FrameRounding = 3.0f;
    style.GrabRounding  = 3.0f;

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion                = VK_API_VERSION_1_4;

    init_info.Instance                                     = ctx.instance;
    init_info.PhysicalDevice                               = ctx.physical_device;
    init_info.Device                                       = ctx.device;
    init_info.QueueFamily                                  = ctx.queues[static_cast<u32>(rhi::queue_kind::gfx)].family;
    init_info.Queue                                        = ctx.queues[static_cast<u32>(rhi::queue_kind::gfx)].queue;
    init_info.PipelineCache                                = VK_NULL_HANDLE;
    init_info.UseDynamicRendering                          = true;
    init_info.MinAllocationSize                            = 1024 * 1024;
    init_info.DescriptorPool                               = VK_NULL_HANDLE;
    init_info.MinImageCount                                = 2;
    init_info.ImageCount                                   = sc_image_count;
    init_info.DescriptorPoolSize                           = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;
    init_info.Allocator                                    = nullptr;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &format,
    };

    if (!ImGui_ImplVulkan_Init(&init_info))
    {
        return error("ImGui_ImplVulkan_Init failed");
    }

    VkImageCreateInfo image_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = format,
        .extent      = {.width = vk_imgui::kAtlasWidth, .height = vk_imgui::kAtlasHeight, .depth = 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const auto atlas_image =
        platform::vk_create_image(ctx.device, image_info, VK_IMAGE_ASPECT_COLOR_BIT, ctx.allocator);
    RESULT_FORWARD_IF_FAILED(atlas_image);

    const auto sampler = platform::vk_create_sampler(
        ctx.device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    RESULT_FORWARD_IF_FAILED(sampler);

    auto vert = vk_shader::load(ctx.device, kVK_ImguiBlitVertShader);
    RESULT_FORWARD_IF_FAILED(vert);
    SUMMON_JANITOR(platform::vk_destroy_shader(ctx.device, *vert));

    auto frag_2d = vk_shader::load(ctx.device, kVK_ImguiBlitFrag2DShader);
    RESULT_FORWARD_IF_FAILED(frag_2d);
    SUMMON_JANITOR(platform::vk_destroy_shader(ctx.device, *frag_2d));

    auto frag_2d_arr = vk_shader::load(ctx.device, kVK_ImguiBlitFrag2DArrayShader);
    RESULT_FORWARD_IF_FAILED(frag_2d_arr);
    SUMMON_JANITOR(platform::vk_destroy_shader(ctx.device, *frag_2d_arr));

    platform::vk_shader pso_2d_shaders[]     = {*vert, *frag_2d};
    platform::vk_shader pso_2d_arr_shaders[] = {*vert, *frag_2d_arr};

    auto pso_2d = platform::vk_pipeline::create_graphics(ctx.device, pso_2d_shaders, COUNT_OF(pso_2d_shaders));
    auto pso_2d_arr =
        platform::vk_pipeline::create_graphics(ctx.device, pso_2d_arr_shaders, COUNT_OF(pso_2d_arr_shaders));
    RESULT_FORWARD_IF_FAILED(pso_2d);
    RESULT_FORWARD_IF_FAILED(pso_2d_arr);

    vk_imgui result {
        .blit2d       = *pso_2d,
        .blit2d_array = *pso_2d_arr,
    };

    result.atlas_data.atlas_image = *atlas_image;
    result.atlas_data.sampler     = *sampler;
    result.atlas_data.imgui_descriptor =
        ImGui_ImplVulkan_AddTexture(*sampler, atlas_image->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return result;
}

void platform::vk_imgui_destroy_context(const platform::vk_context& ctx, vk_imgui& context)
{
    ImGui_ImplVulkan_RemoveTexture(context.atlas_data.imgui_descriptor);

    VK_DESTROY(context.atlas_data.sampler, vkDestroySampler, ctx.device);
    platform::vk_destroy_pipeline(ctx.device, context.blit2d);
    platform::vk_destroy_pipeline(ctx.device, context.blit2d_array);
    platform::vk_destroy_image(ctx.device, ctx.allocator, context.atlas_data.atlas_image);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

