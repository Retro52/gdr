#pragma once

#include <types.hpp>

#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_desc_heap.hpp>

namespace platform
{
    struct d3d12_sampler
    {
        d3d12_view_desc desc;
    };

    d3d12_sampler d3d12_create_sampler(ID3D12Device* device, d3d12_desc_heap& desc_heap, D3D12_FILTER filter,
                                       D3D12_TEXTURE_ADDRESS_MODE address_mode, u32 anisotropic_filter,
                                       D3D12_COMPARISON_FUNC compare_op, vec4 border_color);

    void d3d12_destroy_sampler(d3d12_desc_heap& desc_heap, d3d12_sampler& sampler);
}
