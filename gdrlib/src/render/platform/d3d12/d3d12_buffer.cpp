#pragma once

#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <tracy/Tracy.hpp>

auto platform::d3d12_create_buffer(D3D12MA::Allocator* allocator, const u64 size, const D3D12_HEAP_TYPE heap,
                                   const D3D12_RESOURCE_FLAGS flags) -> result<d3d12_buffer>
{
    ZoneScoped;
    d3d12_buffer result;

    const D3D12_RESOURCE_DESC buf_desc {
        .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width            = size,
        .Height           = 1,
        .DepthOrArraySize = 1,
        .MipLevels        = 1,
        .Format           = DXGI_FORMAT_UNKNOWN,
        .SampleDesc       = {.Count = 1, .Quality = 0},
        .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags            = flags,
    };

    const D3D12MA::ALLOCATION_DESC ma_desc = {.HeapType = heap};

    const bool mapped = (heap == D3D12_HEAP_TYPE_UPLOAD || heap == D3D12_HEAP_TYPE_GPU_UPLOAD);
    const auto state  = mapped ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

    com_ptr<ID3D12Resource> handle;
    D3D12_RETURN_ON_FAIL(
        allocator->CreateResource(&ma_desc, &buf_desc, state, nullptr, &result.allocation, IID_PPV_ARGS(&handle)));

    result.size = size;
    result.va   = handle->GetGPUVirtualAddress();

    if (heap == D3D12_HEAP_TYPE_UPLOAD)
    {
        constexpr D3D12_RANGE kRangeNoRead {.Begin = 0, .End = 0};
        D3D12_RETURN_ON_FAIL(handle->Map(0, &kRangeNoRead, &result.mapped));
    }

    return result;
}

void platform::d3d12_destroy_buffer(d3d12_buffer& buffer)
{
    ZoneScoped;
    if (buffer.mapped)
    {
        constexpr D3D12_RANGE kRangeNoRead {.Begin = 0, .End = 0};
        buffer.allocation->GetResource()->Unmap(0, &kRangeNoRead);
    }

    buffer.allocation.Reset();

    buffer.size   = 0;
    buffer.va     = {};
    buffer.mapped = nullptr;
}

auto platform::d3d12_create_buffer_cbv(ID3D12Device* device, const d3d12_buffer& buffer, d3d12_desc_heap& heap)
    -> d3d12_view_desc
{
    ZoneScoped;
    const D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc {
        .BufferLocation = buffer.va,
        .SizeInBytes    = static_cast<UINT>(buffer.size),
    };

    const d3d12_view_desc alloc_handle = heap.alloc();
    device->CreateConstantBufferView(&cbv_desc, alloc_handle.cpu_handle);

    return alloc_handle;
}

void platform::d3d12_create_buffer_uav(ID3D12Device* device, d3d12_buffer& buffer, d3d12_desc_heap& shader_visible_heap,
                                       d3d12_desc_heap& cpu_heap)
{
    ZoneScoped;
    const D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc {
        .Format        = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer        = {.FirstElement = 0,
                          .NumElements  = static_cast<UINT>(buffer.size / 4),
                          .Flags        = D3D12_BUFFER_UAV_FLAG_RAW},
    };

    buffer.uav     = shader_visible_heap.alloc();
    buffer.uav_cpu = cpu_heap.alloc();

    auto* resource = buffer.allocation->GetResource();
    device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, buffer.uav.cpu_handle);
    device->CreateUnorderedAccessView(resource, nullptr, &uav_desc, buffer.uav_cpu.cpu_handle);
}

void platform::d3d12_destroy_buffer_uav(d3d12_buffer& buffer, d3d12_desc_heap& shader_visible_heap,
                                        d3d12_desc_heap& cpu_heap)
{
    ZoneScoped;
    if (!buffer.uav.cpu_handle.ptr)
    {
        return;
    }

    shader_visible_heap.free(buffer.uav);
    cpu_heap.free(buffer.uav_cpu);

    buffer.uav     = {};
    buffer.uav_cpu = {};
}
