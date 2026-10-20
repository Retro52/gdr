#pragma once

#include <types.hpp>

#include <reflection/enum.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/types.hpp>
#include <result.hpp>

namespace platform
{
    // clang-format off
    REGISTER_FLAGS(d3d12_image_view_type,
        uav = 1 << 0,
        rtv = 1 << 1,
        dsv = 1 << 2,
        srv = 1 << 3
    );
    // clang-format on

    struct d3d12_image_view_desc
    {
        uvec2 mips;
        uvec2 layers;

        DXGI_FORMAT format;
        rhi::image_view_kind kind;
    };

    struct d3d12_image_view
    {
        ID3D12Resource* source = nullptr;

        d3d12_image_view_desc desc;
        d3d12_image_view_type_bits view_types = {};

        union
        {
            struct
            {
                d3d12_view_desc dsv;
                d3d12_view_desc dsv_srv;
            };

            struct
            {
                d3d12_view_desc rtv;
                d3d12_view_desc uav;
                d3d12_view_desc srv;
            };
        } descriptors = {};
    };

    struct d3d12_image
    {
        com_ptr<ID3D12Resource> resource        = nullptr;
        com_ptr<D3D12MA::Allocation> allocation = nullptr;

        D3D12_BARRIER_LAYOUT layout = D3D12_BARRIER_LAYOUT_UNDEFINED;

        d3d12_image_view view;
    };

    struct d3d12_heaps_view
    {
        d3d12_desc_heap& rtv_heap;
        d3d12_desc_heap& dsv_heap;
        d3d12_desc_heap& cbv_srv_uav_heap;
    };

    result<d3d12_image_view> d3d12_create_image_view(ID3D12Device* device, ID3D12Resource* source,
                                                     const d3d12_heaps_view& heaps, const d3d12_image_view_desc& meta,
                                                     d3d12_image_view_type_bits flags);

    void d3d12_destroy_image_view(d3d12_image_view& view, const d3d12_heaps_view& heaps);

    d3d12_image_view_type_bits d3d12_view_types_from_resource_flags(D3D12_RESOURCE_FLAGS flags);

    void d3d12_write_srv(ID3D12Device* device, const d3d12_image_view& view, D3D12_CPU_DESCRIPTOR_HANDLE dst);

    result<d3d12_image> d3d12_create_image(D3D12MA::Allocator* allocator, D3D12_RESOURCE_DIMENSION type,
                                           DXGI_FORMAT format, u32 mips, uvec3 dimensions, D3D12_RESOURCE_FLAGS flags);

    void d3d12_destroy_image(d3d12_image& image);
}
