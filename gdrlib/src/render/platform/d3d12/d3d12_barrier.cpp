#include <render/platform/d3d12/d3d12_barrier.hpp>

D3D12_GLOBAL_BARRIER platform::d3d12_full_barrier()
{
    return {
        .SyncBefore   = D3D12_BARRIER_SYNC_ALL,
        .SyncAfter    = D3D12_BARRIER_SYNC_ALL,
        .AccessBefore = D3D12_BARRIER_ACCESS_COMMON,
        .AccessAfter  = D3D12_BARRIER_ACCESS_COMMON,
    };
}

D3D12_TEXTURE_BARRIER platform::d3d12_initialize_texture_barrier(ID3D12Resource* resource)
{
    return {
        .SyncBefore   = D3D12_BARRIER_SYNC_NONE,
        .SyncAfter    = D3D12_BARRIER_SYNC_NONE,
        .AccessBefore = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .AccessAfter  = D3D12_BARRIER_ACCESS_NO_ACCESS,
        .LayoutBefore = D3D12_BARRIER_LAYOUT_UNDEFINED,
        .LayoutAfter  = D3D12_BARRIER_LAYOUT_COMMON,
        .pResource    = resource,
        .Subresources = {.IndexOrFirstMipLevel = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES},
        .Flags        = D3D12_TEXTURE_BARRIER_FLAG_DISCARD,
    };
}

D3D12_TEXTURE_BARRIER platform::d3d12_restore_texture_barrier(D3D12_TEXTURE_BARRIER barrier)
{
    barrier.SyncBefore   = D3D12_BARRIER_SYNC_ALL;
    barrier.SyncAfter    = D3D12_BARRIER_SYNC_ALL;
    barrier.AccessBefore = barrier.AccessAfter;
    barrier.AccessAfter  = D3D12_BARRIER_ACCESS_COMMON;
    barrier.LayoutBefore = barrier.LayoutAfter;
    barrier.LayoutAfter  = D3D12_BARRIER_LAYOUT_COMMON;
    return barrier;
}

void platform::d3d12_append_texture_barriers(core::heap_array<D3D12_TEXTURE_BARRIER>& barriers,
                                             ID3D12Resource* resource, const D3D12_RESOURCE_DESC& desc,
                                             const D3D12_BARRIER_SUBRESOURCE_RANGE& range,
                                             const D3D12_BARRIER_LAYOUT layout, const D3D12_BARRIER_ACCESS access)
{
    const u32 layers = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;
    const bool valid = resource && range.NumMipLevels && range.NumArraySlices && range.NumPlanes
                    && range.IndexOrFirstMipLevel < desc.MipLevels
                    && range.NumMipLevels <= desc.MipLevels - range.IndexOrFirstMipLevel
                    && range.FirstArraySlice < layers && range.NumArraySlices <= layers - range.FirstArraySlice;

    assert2m(valid, "invalid texture access range");
    if (!valid)
    {
        return;
    }

    for (u32 plane = range.FirstPlane; plane < range.FirstPlane + range.NumPlanes; ++plane)
    {
        for (u32 layer = range.FirstArraySlice; layer < range.FirstArraySlice + range.NumArraySlices; ++layer)
        {
            for (u32 mip = range.IndexOrFirstMipLevel; mip < range.IndexOrFirstMipLevel + range.NumMipLevels; ++mip)
            {
                const u32 subresource = mip + layer * desc.MipLevels + plane * desc.MipLevels * layers;
                barriers.push_back(D3D12_TEXTURE_BARRIER {
                    .SyncBefore   = D3D12_BARRIER_SYNC_ALL,
                    .SyncAfter    = D3D12_BARRIER_SYNC_ALL,
                    .AccessBefore = D3D12_BARRIER_ACCESS_COMMON,
                    .AccessAfter  = access,
                    .LayoutBefore = D3D12_BARRIER_LAYOUT_COMMON,
                    .LayoutAfter  = layout,
                    .pResource    = resource,
                    .Subresources = {.IndexOrFirstMipLevel = subresource, .NumMipLevels = 0},
                });
            }
        }
    }
}

void platform::d3d12_append_texture_copy_barriers(core::heap_array<D3D12_TEXTURE_BARRIER>& barriers,
                                                  ID3D12Resource* resource, const D3D12_RESOURCE_DESC& desc,
                                                  const D3D12_BARRIER_SUBRESOURCE_RANGE& range)
{
    return d3d12_append_texture_barriers(
        barriers, resource, desc, range, D3D12_BARRIER_LAYOUT_COMMON, D3D12_BARRIER_ACCESS_COMMON);
}
