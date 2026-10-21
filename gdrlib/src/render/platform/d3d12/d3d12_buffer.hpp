#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <result.hpp>

namespace platform
{
    struct d3d12_buffer
    {
        u64 size                     = 0;
        D3D12_GPU_VIRTUAL_ADDRESS va = {};

        d3d12_view_desc uav;
        d3d12_view_desc uav_cpu;

        void* mapped                            = nullptr;
        com_ptr<D3D12MA::Allocation> allocation = nullptr;
    };

    result<d3d12_buffer> d3d12_create_buffer(D3D12MA::Allocator* allocator, u64 size, D3D12_HEAP_TYPE heap,
                                             D3D12_RESOURCE_FLAGS flags);

    void d3d12_destroy_buffer(d3d12_buffer& buffer);

    void d3d12_create_buffer_uav(ID3D12Device* device, d3d12_buffer& buffer, d3d12_desc_heap& shader_visible_heap,
                                 d3d12_desc_heap& cpu_heap);

    void d3d12_destroy_buffer_uav(d3d12_buffer& buffer, d3d12_desc_heap& shader_visible_heap,
                                  d3d12_desc_heap& cpu_heap);
}
