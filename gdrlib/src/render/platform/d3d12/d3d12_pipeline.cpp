#include <fs/fs.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <render/platform/d3d12/d3d12_utils.hpp>

result<render::d3d12_shader> render::d3d12_create_shader(const fs::path& shader_path)
{
    ZoneScoped;
    const auto spv_binary = fs::read_file(shader_path);
    RESULT_FORWARD_IF_FAILED(spv_binary);

    const auto d3d12_binary = fs::read_file(shader_path.parent() / shader_path.stem().append(".dxl"));
    RESULT_FORWARD_IF_FAILED(d3d12_binary);

    return render::d3d12_shader {
        .dxil_bytecode = *d3d12_binary,
        .spv_meta      = vk_shader::parse_spirv(*spv_binary),
    };
}

void render::d3d12_destroy_shader(d3d12_shader& shader)
{
    shader.dxil_bytecode.release();
    memset(&shader.spv_meta, 0, sizeof(shader.spv_meta));
}

auto render::d3d12_create_pipeline_compute(ID3D12Device2* device, const d3d12_shader& shader, bool debug,
                                           const d3d12_descriptor_set* desc_set, u32 desc_set_count)
    -> result<d3d12_pipeline>
{
    // D3D12_COMPUTE_PIPELINE_STATE_DESC desc {
    //     .CS    = {.pShaderBytecode = shader.dxil_bytecode.data(), .BytecodeLength = shader.dxil_bytecode.size()},
    //     .Flags = debug ? D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG : D3D12_PIPELINE_STATE_FLAG_NONE,
    // };

    // device->CreateRootSignature();
    // ID3D12RootSignature root_signature {
    //
    // };
    // device->CreateComputePipelineState();

    // Serialize the root signature.
    return error("not implemented");
}

auto render::d3d12_create_pipeline_graphics(ID3D12Device2* device, const d3d12_shader* shaders, const u32 shaders_count,
                                            const d3d12_descriptor_set* desc_set, u32 desc_set_count,
                                            const render::rhi::pso_options& options) -> result<d3d12_pipeline>
{
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    static D3D_ROOT_SIGNATURE_VERSION root_signature_version = [device]() -> D3D_ROOT_SIGNATURE_VERSION
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_support = {};
        D3D_ROOT_SIGNATURE_VERSION versions[]             = {
            D3D_ROOT_SIGNATURE_VERSION_1_2,
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            D3D_ROOT_SIGNATURE_VERSION_1_0,
        };

        for (const auto version : versions)
        {
            feature_support.HighestVersion = version;
            if (SUCCEEDED(device->CheckFeatureSupport(
                    D3D12_FEATURE_ROOT_SIGNATURE, &feature_support, sizeof(feature_support))))
            {
                return feature_support.HighestVersion;
            }
        }

        return D3D_ROOT_SIGNATURE_VERSION_1_0;
    }();

    render::com_ptr<ID3DBlob> err_blob;
    render::com_ptr<ID3DBlob> root_signature_blob;

    D3D12_RETURN_ON_FAIL(D3DX12SerializeVersionedRootSignature(
        &root_signature_desc, root_signature_version, &root_signature_blob, &err_blob));

    render::com_ptr<ID3D12RootSignature> root_signature;
    D3D12_RETURN_ON_FAIL(device->CreateRootSignature(0,
                                                     root_signature_blob->GetBufferPointer(),
                                                     root_signature_blob->GetBufferSize(),
                                                     IID_PPV_ARGS(&root_signature)));

    CD3DX12_PIPELINE_STATE_STREAM2 desc = {};
    desc.pRootSignature                 = root_signature.Get();

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.DSVFormat             = static_cast<DXGI_FORMAT>(d3d12_format_from_vk(options.depth_format));

    assert2(options.color_attachments_count < COUNT_OF(options.color_formats));
    D3D12_RT_FORMAT_ARRAY rt_format_array = {.NumRenderTargets = options.color_attachments_count};
    for (u32 i = 0; i < options.color_attachments_count; ++i)
    {
        rt_format_array.RTFormats[i] = static_cast<DXGI_FORMAT>(d3d12_format_from_vk(options.color_formats[i]));
    }

    desc.RTVFormats = rt_format_array;

    CD3DX12_RASTERIZER_DESC raster(D3D12_DEFAULT);
    raster.FrontCounterClockwise = TRUE;
    raster.CullMode              = D3D12_CULL_MODE_NONE;

    desc.RasterizerState = raster;

    const bool has_depth = options.depth_format != VK_FORMAT_UNDEFINED;
    CD3DX12_DEPTH_STENCIL_DESC1 depth(D3D12_DEFAULT);
    depth.DepthFunc        = D3D12_COMPARISON_FUNC_GREATER;
    depth.DepthEnable      = has_depth && options.flags & rhi::pso_flag::eDepthTest;
    depth.DepthWriteMask   = has_depth && options.flags & rhi::pso_flag::eDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                                                                     : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState = depth;

    for (u32 i = 0; i < shaders_count; ++i)
    {
        D3D12_SHADER_BYTECODE* dst = nullptr;
        switch (shaders[i].spv_meta.stage)
        {
        case VK_SHADER_STAGE_VERTEX_BIT :
            dst = &desc.VS;
            break;
        case VK_SHADER_STAGE_FRAGMENT_BIT :
            dst = &desc.PS;
            break;
        case VK_SHADER_STAGE_TASK_BIT_EXT :
            dst = &desc.AS;
            break;
        case VK_SHADER_STAGE_MESH_BIT_EXT :
            dst = &desc.MS;
            break;
        default :
            break;
        }

        if (dst)
        {
            *dst = {
                .pShaderBytecode = shaders[i].dxil_bytecode.data(),
                .BytecodeLength  = shaders[i].dxil_bytecode.size(),
            };
        }
    }

    const D3D12_PIPELINE_STATE_STREAM_DESC stream_desc {
        .SizeInBytes                   = sizeof(desc),
        .pPipelineStateSubobjectStream = &desc,
    };

    render::com_ptr<ID3D12PipelineState> pipeline;
    D3D12_RETURN_ON_FAIL(device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&pipeline)));

    return d3d12_pipeline {
        .pso            = pipeline,
        .root_signature = root_signature,
        .topology       = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    };
}

void render::d3d12_destroy_pipeline(d3d12_pipeline& pipeline)
{
    pipeline.pso.Reset();
    pipeline.root_signature.Reset();
}
