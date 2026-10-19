#pragma once

#include <fs/fs.hpp>
#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/rhi_pso_options.hpp>
#include <result.hpp>

namespace platform
{

    enum class PSVShaderKind : u8;
    enum class PSVResourceType : u32;
    enum class PSVResourceKind : u32;

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

    struct d3d12_pipeline
    {
        platform::com_ptr<ID3D12PipelineState> pso;
        platform::com_ptr<ID3D12RootSignature> root_signature;

        uvec3 work_group_size             = uvec3(0);
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
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
