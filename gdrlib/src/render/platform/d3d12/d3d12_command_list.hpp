#pragma once

#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <result.hpp>

#include <array>

namespace platform
{
    struct d3d12_context;

    constexpr u32 kD3D12_ShaderVisibleHeapsCount = 2;
    constexpr u64 kD3D12_CommandListScratchSize  = 64 * 1024;  // for cmd_update and similar TBD (push constants?)

    struct d3d12_command_list
    {
        com_ptr<ID3D12CommandList> command_list   = nullptr;
        com_ptr<ID3D12CommandAllocator> allocator = nullptr;

        u64 scratch_head = 0;
        d3d12_buffer scratch;

        vec3 depth_bias_params   = vec3(0.0F);
        rhi::cull_mode cull_mode = rhi::cull_mode::none;
        d3d12_pipeline* pso      = nullptr;

        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> shader_visible_heaps = {nullptr};
    };

    result<d3d12_command_list> d3d12_create_command_list(
        ID3D12Device4* device, D3D12MA::Allocator* allocator, D3D12_COMMAND_LIST_TYPE type,
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps);

    void d3d12_destroy_command_list(d3d12_command_list& command_list);

    u64 d3d12_command_list_scratch_alloc(d3d12_command_list& command_list, u64 size);
}
