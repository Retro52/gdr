#pragma once

#include <cpp/containers/heap_array.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/platform/d3d12/d3d12_fence.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
#include <render/types.hpp>
#include <window.hpp>

namespace platform
{
    // god I'm good with naming things
    constexpr u32 kD3D12CommandQueuesCount  = 3;
    constexpr u32 kD3D12CommandQueueCopy    = 0;
    constexpr u32 kD3D12CommandQueueCompute = 1;
    constexpr u32 kD3D12CommandQueueDirect  = 2;
    constexpr u32 kD3D12RTVHeapMaxDescCount = 64;

    // clang-format off
    REGISTER_FLAGS(swapchain_flag,
        eVsync   = 1 << 0,
        eTearing = 1 << 1,
        COUNT
    );
    // clang-format on

    struct d3d12_context
    {
        com_ptr<ID3D12Device10> device = nullptr;
        com_ptr<IDXGIFactory6> factory = nullptr;
        com_ptr<IDXGIAdapter4> adapter = nullptr;
        com_ptr<ID3D12CommandQueue> queues[kD3D12CommandQueuesCount];
        com_ptr<ID3D12InfoQueue1> info_queue;

        d3d12_tracked_heap<kD3D12RTVHeapMaxDescCount> rtv_descriptor_heap;

        com_ptr<D3D12MA::Allocator> allocator;

        HWND window_handle = nullptr;

        DWORD info_queue_cookie         = 0;
        D3D_SHADER_MODEL shader_model   = D3D_SHADER_MODEL_5_1;
        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        rhi::rendering_features_table enabled_device_features;
    };

    struct d3d12_sc_back_buffer
    {
        d3d12_image image;
        u64 wait_value = 0;
    };

    struct d3d12_swapchain
    {
        u32 frame_counter = 0;
        swapchain_flag_bits flags;

        d3d12_fence sc_sync_fence;
        com_ptr<IDXGISwapChain4> swapchain = nullptr;
        cpp::heap_array<d3d12_sc_back_buffer> back_buffers;
    };

    void d3d12_destroy_context(d3d12_context& ctx);

    result<d3d12_context> d3d12_create_context(const window& window, const rhi::instance_desc& desc);

    void d3d12_destroy_swapchain(d3d12_context& d3d12_context, d3d12_swapchain& swapchain);

    result<d3d12_swapchain> d3d12_create_swapchain(d3d12_context& d3d12_context, u32 format, ivec2 size,
                                                   u32 frames_in_flight, bool vsync);

    result<cpp::heap_array<d3d12_sc_back_buffer>> d3d12_update_back_buffers(platform::d3d12_context& d3d12_context,
                                                                            IDXGISwapChain4* swapchain, u32 count);

}
