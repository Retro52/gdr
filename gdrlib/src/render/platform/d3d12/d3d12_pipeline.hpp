#pragma once

#include <fs/fs.hpp>
#include <render/platform/d3d12/d3d12_buffer.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/rhi_pso_options.hpp>
#include <result.hpp>

namespace platform
{
    enum class PSVShaderKind : u8;
    enum class PSVResourceType : u32;
    enum class PSVResourceKind : u32;

    // clang-format off
    REGISTER_FLAGS(d3d12_pso_dynstates,
        cull_mode_none,
        cull_mode_back,
        cull_mode_front,
        not_set,
        COUNT = not_set
    );
    // clang-format on

    struct d3d12_descriptor_set
    {
        u32 resources_count;
    };

    struct d3d12_shader_resource_binding
    {
        u32 slot;
        u32 size;
        u32 space;

        PSVResourceType resource_type;
        PSVResourceKind resource_kind;
    };

    struct d3d12_shader_meta
    {
        uvec3 threads;
        PSVShaderKind shader_type;

        u32 resources_count : 24 = 0;
        d3d12_shader_resource_binding resources[32];
    };

    struct d3d12_shader
    {
        bytes dxil_bytecode;
        d3d12_shader_meta dxil_meta;
    };

    struct d3d12_pso_param_key
    {
        u32 slot;
        u32 space;
        D3D12_DESCRIPTOR_RANGE_TYPE resource_type;
    };

    struct d3d12_root_signature
    {
        constexpr static u32 kMaxSetSets         = 4;
        constexpr static u32 kMaxSetZeroBindings = 32;
        constexpr static u32 kMaxSetTypes        = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1;
        constexpr static u32 kInvalidDedup       = ~static_cast<u32>(0);

        platform::com_ptr<ID3D12RootSignature> signature = nullptr;
        u32 dedup[kMaxSetSets][kMaxSetTypes][kMaxSetZeroBindings];

        d3d12_root_signature();

        [[nodiscard]] u32 get_bind_index(d3d12_pso_param_key key) const;

        template<typename Func>
        void for_each_type(const u32 space, const u32 slot, Func&& cb) const
        {
            d3d12_pso_param_key key {.slot = slot, .space = space};
            for (u32 i = 0; i < kMaxSetTypes; ++i)
            {
                if (this->dedup[space][i][slot] != kInvalidDedup)
                {
                    key.resource_type = static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(i);
                    cb(key.resource_type, this->get_bind_index(key));
                }
            }
        }
    };

    struct d3d12_pipeline
    {
        d3d12_root_signature rs;
        cpp::heap_array<platform::com_ptr<ID3D12PipelineState>> variants;

        uvec3 work_group_size             = uvec3(0);
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

        [[nodiscard]] bool is_compute() const noexcept;

        [[nodiscard]] u32 get_bind_index(d3d12_pso_param_key key) const;

        void bind(ID3D12GraphicsCommandList9* cmd, rhi::cull_mode cull_mode, vec3 depth_bias_params) const;
    };

    result<d3d12_shader_meta> d3d12_parse_dxil(const bytes& data);

    rhi::shader_stage d3d12_rhi_translate_shader_stage(PSVShaderKind kind);

    result<d3d12_shader> d3d12_create_shader(const fs::path& shader_path);

    void d3d12_destroy_shader(d3d12_shader& shader);

    result<d3d12_pipeline> d3d12_create_pipeline_compute(ID3D12Device2* device, const d3d12_shader& shader);

    result<d3d12_pipeline> d3d12_create_pipeline_graphics(ID3D12Device2* device, const d3d12_shader* shaders,
                                                          u32 shaders_count, const rhi::pso_options& options = {});

    void d3d12_destroy_pipeline(d3d12_pipeline& pipeline);
}
