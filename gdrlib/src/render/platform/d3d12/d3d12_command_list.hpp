#pragma once

#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace render
{
    struct d3d12_context;

    struct d3d12_command_list
    {
        com_ptr<ID3D12CommandList> command_list;
        com_ptr<ID3D12CommandAllocator> allocator;
    };

    result<d3d12_command_list> d3d12_create_command_list(ID3D12Device4* device, D3D12_COMMAND_LIST_TYPE type);

    void d3d12_destroy_command_list(d3d12_command_list& command_list);
}
