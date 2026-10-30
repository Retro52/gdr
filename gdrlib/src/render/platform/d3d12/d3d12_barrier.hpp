#pragma once

#include <cpp/containers/heap_array.hpp>
#include <render/platform/d3d12/d3d12.hpp>

namespace platform
{
    D3D12_GLOBAL_BARRIER d3d12_full_barrier();

    D3D12_TEXTURE_BARRIER d3d12_initialize_texture_barrier(ID3D12Resource* resource);

    D3D12_TEXTURE_BARRIER d3d12_restore_texture_barrier(D3D12_TEXTURE_BARRIER barrier);

    void d3d12_append_texture_barriers(cpp::heap_array<D3D12_TEXTURE_BARRIER>& barriers, ID3D12Resource* resource,
                                       const D3D12_RESOURCE_DESC& desc, const D3D12_BARRIER_SUBRESOURCE_RANGE& range,
                                       D3D12_BARRIER_LAYOUT layout, D3D12_BARRIER_ACCESS access);

    void d3d12_append_texture_copy_barriers(cpp::heap_array<D3D12_TEXTURE_BARRIER>& barriers, ID3D12Resource* resource,
                                            const D3D12_RESOURCE_DESC& desc,
                                            const D3D12_BARRIER_SUBRESOURCE_RANGE& range);

}
