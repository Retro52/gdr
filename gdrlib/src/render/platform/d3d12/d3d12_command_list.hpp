#pragma once

#include <cpp/hash/crc_hash.hpp>
#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <result.hpp>

#include <array>
#include <unordered_map>

namespace platform
{
    struct d3d12_context;

    // clang-format off
    REGISTER_ENUM_T(d3d12_command_signature_type, u8,
        none,
        dispatch_indirect,
        draw_indexed_indirect,
        draw_mesh_task_indirect,
        COUNT
    );
    // clang-format on

    struct d3d12_command_signature_key
    {
        u8 stride                         = 0;
        d3d12_command_signature_type type = d3d12_command_signature_type::none;

        [[nodiscard]] u32 hash() const { return cpp::crc::crc32(reinterpret_cast<const char*>(this), sizeof(*this)); }
    };

    constexpr u32 kD3D12_ShaderVisibleHeapsCount = 2;
    constexpr u64 kD3D12_CommandListScratchSize  = 64 * 1024;  // for cmd_update and similar TBD (push constants?)

    struct d3d12_command_list
    {
        ID3D12Device* device = nullptr;

        com_ptr<ID3D12CommandList> command_list   = nullptr;
        com_ptr<ID3D12CommandAllocator> allocator = nullptr;

        u64 scratch_head = 0;
        d3d12_buffer scratch;

        vec3 depth_bias_params   = vec3(0.0F);
        rhi::cull_mode cull_mode = rhi::cull_mode::none;
        d3d12_pipeline* pso      = nullptr;

        std::unordered_map<u32, com_ptr<ID3D12CommandSignature>> command_signatures;
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> shader_visible_heaps = {};

        ID3D12CommandSignature* get_signature(d3d12_command_signature_key signature);
    };

    result<d3d12_command_list> d3d12_create_command_list(
        ID3D12Device4* device, D3D12MA::Allocator* allocator, D3D12_COMMAND_LIST_TYPE type,
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps);

    void d3d12_destroy_command_list(d3d12_command_list& command_list);

    u64 d3d12_command_list_scratch_alloc(d3d12_command_list& command_list, u64 size);
}
