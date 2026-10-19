#pragma once

#include <types.hpp>

#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace platform
{
    struct d3d12_image_view
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {};
        D3D12_CPU_DESCRIPTOR_HANDLE gpu_handle = {};
    };

    struct d3d12_image
    {
        com_ptr<ID3D12Resource> resource        = nullptr;
        com_ptr<D3D12MA::Allocation> allocation = nullptr;

        D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_UNDEFINED;

        d3d12_image_view rtv;
        d3d12_image_view srv;
        d3d12_image_view uav;
    };

    result<d3d12_image> d3d12_create_image(D3D12MA::Allocator* allocator, D3D12_RESOURCE_DIMENSION type,
                                           DXGI_FORMAT format, u32 mips, uvec3 dimensions, D3D12_RESOURCE_FLAGS flags);

    void d3d12_destroy_image(d3d12_image& image);
}
