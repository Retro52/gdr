#include <render/platform/d3d12/d3d12_imgui.hpp>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_sdl3.h>
#include <cmath>
#include <imgui.h>

void platform::d3d12_imgui_init(d3d12_context& ctx, SDL_Window* window, const u32 sc_image_count)
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

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForD3D(window);

    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device                  = ctx.device.Get();
    init_info.CommandQueue            = ctx.queues[platform::kD3D12_CommandQueueDirect].Get();
    init_info.NumFramesInFlight       = sc_image_count;
    init_info.RTVFormat               = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat               = DXGI_FORMAT_UNKNOWN;
    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
    init_info.UserData          = &ctx.cbv_srv_uav_descriptor_heap;
    init_info.SrvDescriptorHeap = ctx.cbv_srv_uav_descriptor_heap.heap.Get();

    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
                                        D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
                                        D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        auto& heap      = *static_cast<platform::d3d12_desc_heap*>(info->UserData);
        const auto view = heap.alloc();
        *out_cpu_handle = view.cpu_handle;
        *out_gpu_handle = view.gpu_handle;
    };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
                                       const D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
                                       const D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
        auto& heap = *static_cast<platform::d3d12_desc_heap*>(info->UserData);

        const d3d12_view_desc desc {
            .cpu_handle = cpu_handle,
            .gpu_handle = gpu_handle,
        };

        heap.free(desc);
    };
    ImGui_ImplDX12_Init(&init_info);
}

void platform::d3d12_imgui_shutdown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void platform::d3d12_imgui_new_frame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void platform::d3d12_imgui_draw_frame(const d3d12_command_list& cmd, const d3d12_image_view& rtv)
{
    ImGui::Render();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = rtv.source;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmd.command_list->ResourceBarrier(1, &barrier);

    // Render Dear ImGui graphics
    const auto rtv_desc = rtv.rtv().cpu_handle;
    cmd.command_list->OMSetRenderTargets(1, &rtv_desc, FALSE, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd.command_list.Get());
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    cmd.command_list->ResourceBarrier(1, &barrier);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
