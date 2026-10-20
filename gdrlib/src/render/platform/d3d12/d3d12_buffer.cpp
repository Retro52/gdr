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
