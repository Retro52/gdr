#pragma once

#include <core/containers/local_array.hpp>
#include <pod_types.hpp>
#include <render/platform/d3d12/d3d12.hpp>
#include <render/platform/d3d12/d3d12_blit.hpp>
#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_desc_heap.hpp>
#include <render/platform/d3d12/d3d12_image.hpp>
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

        [[nodiscard]] u32 hash() const { return static_cast<u32>(stride) << 8 | static_cast<u32>(type); }
    };

    constexpr u32 kD3D12_ShaderVisibleHeapsCount = 2;
    constexpr u32 kD3D12_CommandListScratchDescs = 64;         // transient blit views
    constexpr u64 kD3D12_CommandListScratchSize  = 64 * 1024;  // for cmd_update and similar TBD (push constants?)

    struct d3d12_binding_expected_layout
    {
        D3D12_BARRIER_LAYOUT layout;
        D3D12_BARRIER_ACCESS access;

        const d3d12_image_view* view;
    };

    struct d3d12_command_list
    {
        u32 pc_size     = 0;
        u8 pc_data[252] = {};

        ID3D12Device* device = nullptr;

        com_ptr<ID3D12CommandAllocator> allocator         = nullptr;
        com_ptr<ID3D12GraphicsCommandList10> command_list = nullptr;

        u64 scratch_head = 0;
        d3d12_buffer scratch;

        u64 scratch_descriptors_head = 0;
        d3d12_view_desc_range scratch_descriptors;
        const d3d12_blit_pipeline* blit = nullptr;

        vec3 depth_bias_params   = vec3(0.0F);
        rhi::cull_mode cull_mode = rhi::cull_mode::none;
        d3d12_pipeline* pso      = nullptr;

        core::heap_array<D3D12_TEXTURE_BARRIER> attachment_barriers;
        core::local_array<d3d12_binding_expected_layout, 8> storage_views;

        std::unordered_map<u32, com_ptr<ID3D12CommandSignature>> command_signatures;
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> shader_visible_heaps = {};

        void free_scratch(d3d12_desc_heap& heap) const;

        ID3D12CommandSignature* get_signature(d3d12_command_signature_key signature);
    };

    result<d3d12_command_list> d3d12_create_command_list(
        ID3D12Device4* device, D3D12MA::Allocator* allocator, D3D12_COMMAND_LIST_TYPE type,
        std::array<ID3D12DescriptorHeap*, kD3D12_ShaderVisibleHeapsCount> heaps);

    void d3d12_destroy_command_list(d3d12_command_list& command_list);

    u64 d3d12_command_list_scratch_alloc(d3d12_command_list& command_list, u64 size, u64 align = 1);
}
