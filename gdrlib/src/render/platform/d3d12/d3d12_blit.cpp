#include <render/platform/d3d12/d3d12_blit.hpp>
#include <render/platform/d3d12/d3d12_blit_shader.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <tracy/Tracy.hpp>

auto platform::d3d12_create_blit_pipeline(ID3D12Device* device) -> result<d3d12_blit_pipeline>
{
    ZoneScoped;
    d3d12_blit_pipeline result;

    D3D12_RETURN_ON_FAIL(
        device->CreateRootSignature(0, kD3D12_BlitShader, sizeof(kD3D12_BlitShader), IID_PPV_ARGS(&result.signature)));

    const D3D12_COMPUTE_PIPELINE_STATE_DESC desc {
        .pRootSignature = result.signature.Get(),
        .CS             = {.pShaderBytecode = kD3D12_BlitShader, .BytecodeLength = sizeof(kD3D12_BlitShader)},
    };

    D3D12_RETURN_ON_FAIL(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&result.pso)));
    return result;
}
