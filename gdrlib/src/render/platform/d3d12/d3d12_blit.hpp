#pragma once

#include <render/platform/d3d12/d3d12.hpp>
#include <result.hpp>

namespace platform
{
    struct d3d12_blit_pipeline
    {
        com_ptr<ID3D12RootSignature> signature = nullptr;
        com_ptr<ID3D12PipelineState> pso       = nullptr;
    };

    result<d3d12_blit_pipeline> d3d12_create_blit_pipeline(ID3D12Device* device);
}
