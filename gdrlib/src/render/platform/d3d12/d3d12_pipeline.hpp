#pragma once

#include <render/platform/d3d12/d3d12_device.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/rhi_pso_options.hpp>
#include <result.hpp>

namespace render
{
    struct d3d12_descriptor_set
    {
    };

    struct d3d12_shader
    {
        bytes dxil_bytecode;
        vk_shader::shader_meta spv_meta;
    };

    struct d3d12_pipeline
    {
        render::com_ptr<ID3D12PipelineState> pso;
        render::com_ptr<ID3D12RootSignature> root_signature;

        D3D12_PRIMITIVE_TOPOLOGY topology;
    };

    result<d3d12_shader> d3d12_create_shader(const fs::path& shader_path);

    void d3d12_destroy_shader(d3d12_shader& shader);

    result<d3d12_pipeline> d3d12_create_pipeline_compute(ID3D12Device2* device, const d3d12_shader& shader, bool debug,
                                                         const d3d12_descriptor_set* desc_set = nullptr,
                                                         u32 desc_set_count                   = 0);

    result<d3d12_pipeline> d3d12_create_pipeline_graphics(ID3D12Device2* device, const d3d12_shader* shaders,
                                                          u32 shaders_count,
                                                          const d3d12_descriptor_set* desc_set    = nullptr,
                                                          u32 desc_set_count                      = 0,
                                                          const render::rhi::pso_options& options = {});

    void d3d12_destroy_pipeline(d3d12_pipeline& pipeline);
}
