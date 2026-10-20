#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

#include <array>

namespace platform
{
    struct d3d12_context;

    constexpr u32 kD3D12_ShaderVisibleHeapsCount = 2;

    struct d3d12_command_list
    {
        com_ptr<ID3D12CommandList> command_list   = nullptr;
        com_ptr<ID3D12CommandAllocator> allocator = nullptr;

        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> shader_visible_heaps = {nullptr};
    };

    result<d3d12_command_list> d3d12_create_command_list(
        ID3D12Device4* device, D3D12_COMMAND_LIST_TYPE type,
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps);

    void d3d12_destroy_command_list(d3d12_command_list& command_list);
}
