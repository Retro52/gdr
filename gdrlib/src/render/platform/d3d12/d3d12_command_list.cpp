#include <assert2.hpp>
#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

auto platform::d3d12_create_command_list(ID3D12Device4* device, D3D12MA::Allocator* allocator,
                                         const D3D12_COMMAND_LIST_TYPE type,
                                         const std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps)
    -> result<d3d12_command_list>
{
    ZoneScoped;
    d3d12_command_list result;
    if (type != D3D12_COMMAND_LIST_TYPE_COPY)
    {
        result.shader_visible_heaps = heaps;
    }

    D3D12_RETURN_ON_FAIL(device->CreateCommandAllocator(type, IID_PPV_ARGS(&result.allocator)));
    D3D12_RETURN_ON_FAIL(
        device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&result.command_list)));

    auto scratch = d3d12_create_buffer(
        allocator, kD3D12_CommandListScratchSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);
    RESULT_FORWARD_IF_FAILED(scratch);

    result.scratch = *scratch;
    return result;
}

void platform::d3d12_destroy_command_list(d3d12_command_list& command_list)
{
    d3d12_destroy_buffer(command_list.scratch);
    command_list.scratch_head = 0;

    command_list.allocator.Reset();
    command_list.command_list.Reset();
}

u64 platform::d3d12_command_list_scratch_alloc(d3d12_command_list& command_list, const u64 size)
{
    ZoneScoped;
    const u64 offset = command_list.scratch_head;

    assert2m(offset + size <= kD3D12_CommandListScratchSize, "command list scratch exhausted");
    if (offset + size > kD3D12_CommandListScratchSize)
    {
        return kD3D12_CommandListScratchSize;
    }

    command_list.scratch_head = (offset + size + 3) & ~static_cast<u64>(3);
    return offset;
}
