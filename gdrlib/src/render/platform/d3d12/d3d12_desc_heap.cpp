#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

bool platform::d3d12_desc_heap_type_is_shader_visible(const D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
}

auto platform::d3d12_create_desc_heap(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_TYPE type,
                                      const u32 num_descriptors, const bool force_cpu_only) -> result<d3d12_desc_heap>
{
    ZoneScoped;
    d3d12_desc_heap result(num_descriptors);

    const bool is_shader_visible = !force_cpu_only && d3d12_desc_heap_type_is_shader_visible(type);

    const D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type           = type,
        .NumDescriptors = num_descriptors,
        .Flags = is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };

    D3D12_RETURN_ON_FAIL(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&result.heap)));

    result.stride           = device->GetDescriptorHandleIncrementSize(type);
    result.start.cpu_handle = result.heap->GetCPUDescriptorHandleForHeapStart();
    result.start.gpu_handle =
        is_shader_visible ? result.heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE {};

    return result;
}

void platform::d3d12_destroy_desc_heap(d3d12_desc_heap& heap)
{
    heap.heap.Reset();
}
