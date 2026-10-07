#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

auto render::d3d12_create_desc_heap(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_TYPE type,
                                    const u32 num_descriptors) -> result<d3d12_desc_heap>
{
    ZoneScoped;
    d3d12_desc_heap result;

    const D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type           = type,
        .NumDescriptors = num_descriptors,
    };

    D3D12_RETURN_ON_FAIL(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&result.heap)));
    result.cpu_handle         = result.heap->GetCPUDescriptorHandleForHeapStart();
    result.cpu_increment_size = device->GetDescriptorHandleIncrementSize(type);
    return result;
}

void render::d3d12_destroy_desc_heap(d3d12_desc_heap& heap)
{
    heap.heap.Reset();
}
