#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

auto render::create_desc_heap(const render::d3d12_context& ctx, D3D12_DESCRIPTOR_HEAP_TYPE type,
                              uint32_t num_descriptors) -> result<d3d12_desc_heap>
{
    ZoneScoped;
    render::com_ptr<ID3D12DescriptorHeap> heap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type           = type,
        .NumDescriptors = num_descriptors,
    };

    D3D12_RETURN_ON_FAIL(ctx.device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
    return heap;
}

void render::destroy_desc_heap(d3d12_desc_heap& heap)
{
    heap.Reset();
}
