#pragma once

#include <cpp/containers/heap_array.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <render/types.hpp>
#include <window.hpp>

namespace render
{
    struct d3d12_context
    {
        com_ptr<ID3D12Device10> device = nullptr;
        com_ptr<IDXGIFactory6> factory = nullptr;
        com_ptr<IDXGIAdapter4> adapter = nullptr;
        com_ptr<ID3D12CommandQueue> queues[rhi::kQueueTypesCount];
        com_ptr<ID3D12InfoQueue1> info_queue;

        com_ptr<D3D12MA::Allocator> allocator;

        DWORD info_queue_cookie         = 0;
        HWND window_handle              = nullptr;
        D3D_SHADER_MODEL shader_model   = D3D_SHADER_MODEL_5_1;
        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        rhi::rendering_features_table enabled_device_features;
    };

    struct d3d12_swapchain
    {
        com_ptr<IDXGISwapChain4> swapchain = nullptr;
        cpp::heap_array<com_ptr<ID3D12Resource>> images;
    };

    void d3d12_destroy_context(d3d12_context& ctx);

    result<d3d12_context> d3d12_create_context(const window& window, const rhi::instance_desc& desc);

    void d3d12_destroy_swapchain(const d3d12_context& d3d12_context, d3d12_swapchain& swapchain);

    result<d3d12_swapchain> d3d12_create_swapchain(const d3d12_context& d3d12_context, u32 format, ivec2 size,
                                                   u32 frames_in_flight, bool vsync);
}
