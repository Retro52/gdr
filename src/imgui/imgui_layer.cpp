#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui/imgui_layer.hpp>

imgui_layer::imgui_layer(const window& window, const render::vk_renderer& renderer)
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
}

void imgui_layer::begin_frame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void imgui_layer::end_frame(const render::vk_renderer& renderer)
{
    ImGui::Render();

    renderer.submit(
        [&](VkCommandBuffer command_buffer)
        {
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
        });

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
