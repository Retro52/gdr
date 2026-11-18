#include <assert2.hpp>
#include <render/platform/d3d12/d3d12_command_list.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>

static D3D12_INDIRECT_ARGUMENT_TYPE d3d12_get_indirect_arg_type(const platform::d3d12_command_signature_type type)
{
    switch (type)
    {
    case platform::d3d12_command_signature_type::dispatch_indirect :
        return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    case platform::d3d12_command_signature_type::draw_mesh_task_indirect :
    case platform::d3d12_command_signature_type::draw_mesh_task_indirect_draw_id :
        return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
    default :
    case platform::d3d12_command_signature_type::draw_indexed_indirect :
        return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
    }
}

void platform::d3d12_command_list::free_scratch(d3d12_desc_heap& heap) const
{
    if (this->scratch_descriptors.count > 0)
    {
        heap.free(this->scratch_descriptors.start, this->scratch_descriptors.count);
    }
}

ID3D12CommandSignature* platform::d3d12_command_list::get_signature(const d3d12_command_signature_key signature)
{
    if (const auto it = this->command_signatures.find(signature); it != this->command_signatures.end())
    {
        return it->second.Get();
    }

    const bool with_draw_id = signature.type == d3d12_command_signature_type::draw_mesh_task_indirect_draw_id;

    u32 args_count = 0;
    D3D12_INDIRECT_ARGUMENT_DESC args[2] {};
    if (with_draw_id)
    {
        args[args_count].Type                             = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        args[args_count].Constant.RootParameterIndex      = signature.root_index;
        args[args_count].Constant.DestOffsetIn32BitValues = 0;
        args[args_count].Constant.Num32BitValuesToSet     = 1;
        ++args_count;
    }
    args[args_count++].Type = d3d12_get_indirect_arg_type(signature.type);

    const D3D12_COMMAND_SIGNATURE_DESC desc {
        .ByteStride       = signature.stride,
        .NumArgumentDescs = args_count,
        .pArgumentDescs   = args,
        .NodeMask         = 0,
    };

    auto& new_entry = this->command_signatures[signature];
    D3D12_ASSERT_ON_FAIL(this->device->CreateCommandSignature(
        &desc, with_draw_id ? signature.root_signature : nullptr, IID_PPV_ARGS(&new_entry)));

    return new_entry.Get();
}

auto platform::d3d12_create_command_list(ID3D12Device4* device, D3D12MA::Allocator* allocator,
                                         const D3D12_COMMAND_LIST_TYPE type,
                                         const std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps)
    -> result<d3d12_command_list>
{
    ZoneScoped;
    d3d12_command_list result {.device = device};
    if (type != D3D12_COMMAND_LIST_TYPE_COPY)
    {
        result.shader_visible_heaps = heaps;
    }

    D3D12_RETURN_ON_FAIL(device->CreateCommandAllocator(type, IID_PPV_ARGS(&result.allocator)));
    D3D12_RETURN_ON_FAIL(
        device->CreateCommandList1(0, type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&result.command_list)));

    auto scratch =
        d3d12_create_buffer(allocator, kD3D12_CommandListScratchSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE);
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

u64 platform::d3d12_command_list_scratch_alloc(d3d12_command_list& command_list, const u64 size, const u64 align)
{
    ZoneScoped;

    assert2m((align & (align - 1)) == 0, "align must be a power of two");
    const u64 offset = (command_list.scratch_head + align - 1) & ~(align - 1);

    assert2m(offset + size <= kD3D12_CommandListScratchSize, "command list scratch exhausted");
    if (offset + size > kD3D12_CommandListScratchSize)
    {
        return kD3D12_CommandListScratchSize;
    }

    command_list.scratch_head = (offset + size + 3) & ~static_cast<u64>(3);
    return offset;
}
