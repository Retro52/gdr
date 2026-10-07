#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace render
{
    struct d3d12_context;

    struct d3d12_fence
    {
        HANDLE event               = nullptr;
        com_ptr<ID3D12Fence> fence = nullptr;
    };

    result<d3d12_fence> d3d12_create_fence(ID3D12Device* device, u64 initial_value);

    void d3d12_signal_fence(const d3d12_fence& fence, ID3D12CommandQueue* queue, u64 value);

    void d3d12_wait_on_fence(const d3d12_fence& fence, u64 value, u64 timeout = UINT64_MAX);

    void d3d12_destroy_fence(d3d12_fence& fence);
}
