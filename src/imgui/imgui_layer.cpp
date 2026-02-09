#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui/imgui_layer.hpp>

imgui_layer::imgui_layer(const window& window, const render::vk_renderer& renderer)
    : m_renderer(renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplSDL3_InitForVulkan(window.get_native_handle().window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    // init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion,
    // otherwise will default to header version.
    auto& context   = renderer.get_context();
    auto& swapchain = renderer.get_swapchain();

    init_info.Instance                                     = context.instance;
    init_info.PhysicalDevice                               = context.physical_device;
    init_info.Device                                       = context.device;
    init_info.QueueFamily                                  = context.queues[render::queue_kind::eGfx].family;
    init_info.Queue                                        = context.queues[render::queue_kind::eGfx].queue;
    init_info.PipelineCache                                = VK_NULL_HANDLE;
    init_info.UseDynamicRendering                          = true;
    init_info.MinAllocationSize                            = 1024 * 1024;
    init_info.DescriptorPool                               = VK_NULL_HANDLE;
    init_info.MinImageCount                                = 2;
    init_info.ImageCount                                   = renderer.get_frames_in_flight();
    init_info.DescriptorPoolSize                           = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    init_info.Allocator                                    = nullptr;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swapchain.surface_format.format,
        .depthAttachmentFormat   = swapchain.depth_format,
    };

    ImGui_ImplVulkan_Init(&init_info);

    VkImageCreateInfo image_info {
        .sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType   = VK_IMAGE_TYPE_2D,
        .format      = VK_FORMAT_B8G8R8A8_SRGB,
        .extent      = {kAtlasWidth, kAtlasHeight, 1},
        .mipLevels   = 1,
        .arrayLayers = 1,
        .samples     = VK_SAMPLE_COUNT_1_BIT,
        .tiling      = VK_IMAGE_TILING_OPTIMAL,
        .usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    m_atlas_data.atlas_image = *render::create_image(
        renderer.get_context().device, image_info, VK_IMAGE_ASPECT_COLOR_BIT, renderer.get_context().allocator);

    m_atlas_data.sampler = *render::create_sampler(renderer.get_context().device,
                                                   VK_FILTER_NEAREST,
                                                   VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                                   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    m_atlas_data.imgui_descriptor = ImGui_ImplVulkan_AddTexture(
        m_atlas_data.sampler, m_atlas_data.atlas_image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    render::vk_shader shaders[] = {
        *render::vk_shader::load(renderer, "../shaders/bin/imgui_blit.vert.spv"),
        *render::vk_shader::load(renderer, "../shaders/bin/imgui_blit.frag.spv"),
    };

    m_blit_pipeline = *render::vk_pipeline::create_graphics(renderer, shaders, COUNT_OF(shaders));
}

imgui_layer::~imgui_layer()
{
    auto& context = m_renderer.get_context();

    ImGui_ImplVulkan_RemoveTexture(m_atlas_data.imgui_descriptor);
    vkDestroySampler(context.device, m_atlas_data.sampler, nullptr);
    render::destroy_image(context.device, context.allocator, m_atlas_data.atlas_image);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void imgui_layer::begin_frame(const render::vk_renderer& renderer)
{
    m_atlas_data.reset_cursor();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    VkRenderingAttachmentInfo color_attachment_info {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = renderer.get_frame_swapchain_image().image_view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {
                        .color = {0.0F, 0.0F, 0.0F, 1.0F},
                        }
    };

    const VkRenderingInfo rendering_info {.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
                                          .renderArea           = renderer.get_scissor(),
                                          .layerCount           = 1,
                                          .colorAttachmentCount = 1,
                                          .pColorAttachments    = &color_attachment_info};

    renderer.submit(
        [&](VkCommandBuffer cmd)
        {
            vkCmdBeginRendering(cmd, &rendering_info);
        });
}

void imgui_layer::end_frame(const render::vk_renderer& renderer)
{
    if (ImGui::CollapsingHeader("pick into the atlas data"))
    {
        ImGui::Image(m_atlas_data.imgui_descriptor,
                     {ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x});
    }

    ImGui::Render();
    renderer.submit(
        [&, this](VkCommandBuffer cmd)
        {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
            vkCmdEndRendering(cmd);
            this->flush_pending(cmd);
        });

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool imgui_layer::allocate_region(u32 w, u32 h, VkOffset2D& out_offset)
{
    auto& atlas = m_atlas_data;
    if (atlas.cursor_x + w > kAtlasWidth)
    {
        atlas.cursor_y += atlas.cursor_row;
        atlas.cursor_x   = 0;
        atlas.cursor_row = 0;
    }

    if (atlas.cursor_y + h > kAtlasHeight)
    {
        return false;
    }

    out_offset.x = static_cast<i32>(atlas.cursor_x);
    out_offset.y = static_cast<i32>(atlas.cursor_y);

    atlas.cursor_x += w;
    atlas.cursor_row = std::max(atlas.cursor_row, h);
    return true;
}

void imgui_layer::image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv, ImVec2 size)
{
    image_impl(image, view, size, {uv.x, uv.y}, {uv.z, uv.w}, src_layout, VK_IMAGE_ASPECT_COLOR_BIT);
}

void imgui_layer::depth_image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv, ImVec2 size)
{
    image_impl(image, view, size, {uv.x, uv.y}, {uv.z, uv.w}, src_layout, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void imgui_layer::image_impl(VkImage image, VkImageView view, ImVec2 size, ImVec2 uv0, ImVec2 uv1,
                             VkImageLayout src_layout, VkImageAspectFlags aspect)
{
    VkOffset2D offset;
    if (!allocate_region(static_cast<u32>(size.x), static_cast<u32>(size.y), offset))
    {
        ImGui::Text("atlas full :(");
        return;
    }

    m_pending_uploads.push_back({
        .img        = image,
        .view       = view,
        .offset     = offset,
        .extent     = {static_cast<u32>(size.x), static_cast<u32>(size.y)},
        .src_layout = src_layout,
        .aspect     = aspect,
    });

    ImVec2 atlas_uv0 {static_cast<f32>(offset.x) / kAtlasWidth, static_cast<f32>(offset.y) / kAtlasHeight};
    ImVec2 atlas_uv1 {(offset.x + size.x) / kAtlasWidth, (offset.y + size.y) / kAtlasHeight};

    uv0.x = std::lerp(atlas_uv0.x, atlas_uv1.x, uv0.x);
    uv0.y = std::lerp(atlas_uv0.y, atlas_uv1.y, uv0.y);
    uv1.x = std::lerp(atlas_uv0.x, atlas_uv1.x, uv1.x);
    uv1.y = std::lerp(atlas_uv0.y, atlas_uv1.y, uv1.y);

    ImGui::Image(m_atlas_data.imgui_descriptor, size, uv0, uv1);
}

void imgui_layer::flush_pending(const VkCommandBuffer cmd)
{
    if (m_pending_uploads.empty())
    {
        return;
    }

    std::vector<VkImageMemoryBarrier2> barriers(m_pending_uploads.size());

    for (u32 i = 0; i < m_pending_uploads.size(); ++i)
    {
        const auto& req = m_pending_uploads[i];
        barriers[i]     = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .srcAccessMask    = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout        = req.src_layout,
                .newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .image            = req.img,
                .subresourceRange = {req.aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };
    }

    VkDependencyInfo dep {
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = static_cast<u32>(barriers.size()),
        .pImageMemoryBarriers    = barriers.data(),
    };
    vkCmdPipelineBarrier2(cmd, &dep);

    render::transition_image(
        cmd, m_atlas_data.atlas_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderingAttachmentInfo color_attachment {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = m_atlas_data.atlas_image.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo rendering_info {
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{0, 0}, {kAtlasWidth, kAtlasHeight}},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_attachment,
    };

    vkCmdBeginRendering(cmd, &rendering_info);
    m_blit_pipeline.bind(cmd);

    for (const auto& req : m_pending_uploads)
    {
        VkViewport viewport {
            .x        = static_cast<f32>(req.offset.x),
            .y        = static_cast<f32>(req.offset.y),
            .width    = static_cast<f32>(req.extent.width),
            .height   = static_cast<f32>(req.extent.height),
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };
        VkRect2D scissor {req.offset, req.extent};

        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        struct push_constants
        {
            u32 is_depth;
        } pc {.is_depth = req.aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? 1u : 0u};

        m_blit_pipeline.push_constant(cmd, pc);

        render::vk_descriptor_info descriptor {
            m_atlas_data.sampler, req.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        m_blit_pipeline.push_descriptor_set(cmd, &descriptor);

        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);

    // Back to original
    for (u32 i = 0; i < m_pending_uploads.size(); ++i)
    {
        const auto& req = m_pending_uploads[i];
        barriers[i]     = {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask    = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask    = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .oldLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout        = req.src_layout,
                .image            = req.img,
                .subresourceRange = {req.aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };
    }

    dep.pImageMemoryBarriers    = barriers.data();
    dep.imageMemoryBarrierCount = static_cast<u32>(barriers.size());
    vkCmdPipelineBarrier2(cmd, &dep);

    m_pending_uploads.clear();
    render::transition_image(cmd,
                             m_atlas_data.atlas_image.image,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
