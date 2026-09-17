#pragma once

#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <result.hpp>

namespace render
{
    using d3d12_desc_heap = com_ptr<ID3D12DescriptorHeap>;
    result<d3d12_desc_heap> create_desc_heap(const render::d3d12_context& ctx, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             uint32_t num_descriptors);

    void destroy_desc_heap(d3d12_desc_heap& heap);
}
