#include <fs/fs.hpp>
#include <render/platform/d3d12/d3d12_error.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>
#include <render/platform/d3d12/d3d12_utils.hpp>

#include <array>

namespace platform
{
    enum class PSVShaderKind : u8
    {
        Pixel [[maybe_unused]],
        Vertex [[maybe_unused]],
        Geometry [[maybe_unused]],
        Hull [[maybe_unused]],
        Domain [[maybe_unused]],
        Compute [[maybe_unused]],
        Library [[maybe_unused]],
        RayGeneration [[maybe_unused]],
        Intersection [[maybe_unused]],
        AnyHit [[maybe_unused]],
        ClosestHit [[maybe_unused]],
        Miss [[maybe_unused]],
        Callable [[maybe_unused]],
        Mesh [[maybe_unused]],
        Amplification [[maybe_unused]],
        Node [[maybe_unused]],
        Invalid [[maybe_unused]],
    };

    enum class PSVResourceType : u32
    {
        Invalid [[maybe_unused]],
        Sampler [[maybe_unused]],
        CBV [[maybe_unused]],
        SRVTyped [[maybe_unused]],
        SRVRaw [[maybe_unused]],
        SRVStructured [[maybe_unused]],
        UAVTyped [[maybe_unused]],
        UAVRaw [[maybe_unused]],
        UAVStructured [[maybe_unused]],
        UAVStructuredWithCounter [[maybe_unused]]
    };

    enum class PSVResourceKind : u32
    {
        Invalid [[maybe_unused]],
        Texture1D [[maybe_unused]],
        Texture2D [[maybe_unused]],
        Texture2DMS [[maybe_unused]],
        Texture3D [[maybe_unused]],
        TextureCube [[maybe_unused]],
        Texture1DArray [[maybe_unused]],
        Texture2DArray [[maybe_unused]],
        Texture2DMSArray [[maybe_unused]],
        TextureCubeArray [[maybe_unused]],
        TypedBuffer [[maybe_unused]],
        RawBuffer [[maybe_unused]],
        StructuredBuffer [[maybe_unused]],
        CBuffer [[maybe_unused]],
        Sampler [[maybe_unused]],
        TBuffer [[maybe_unused]],
        RTAccelerationStructure [[maybe_unused]],
        FeedbackTexture2D [[maybe_unused]],
        FeedbackTexture2DArray [[maybe_unused]],
        NumEntries [[maybe_unused]]
    };
}

namespace
{
    enum class PSVStructVersion : u32
    {
        notset,
        v1,
        v2,
        v3,
        v4,
        unrecognized,
    };

    struct PSVRuntimeInfo_v1
    {
        [[maybe_unused]] u8 Union[16];
        [[maybe_unused]] u32 MinimumExpectedWaveLaneCount;  // minimum lane count required, 0 if unused
        [[maybe_unused]] u32 MaximumExpectedWaveLaneCount;  // maximum lane count required, 0xffffffff if unused
    };

    struct PSVRuntimeInfo_v2 : PSVRuntimeInfo_v1
    {
        [[maybe_unused]] platform::PSVShaderKind Kind;
        [[maybe_unused]] u8 UsesViewId;

        [[maybe_unused]] u16 MaxVertexCount;  // union: GS max vertices / HS-DS-MS vector count
        [[maybe_unused]] u8 SigInputElements;
        [[maybe_unused]] u8 SigOutputElements;
        [[maybe_unused]] u8 SigPatchConstOrPrimElements;
        [[maybe_unused]] u8 SigInputVectors;
        [[maybe_unused]] u8 SigOutputVectors[4];  // one per GS stream
    };

    struct PSVRuntimeInfo_v3 : PSVRuntimeInfo_v2
    {
        [[maybe_unused]] u32 NumThreadsX;
        [[maybe_unused]] u32 NumThreadsY;
        [[maybe_unused]] u32 NumThreadsZ;
    };

    struct PSVRuntimeInfo_v4 : PSVRuntimeInfo_v3
    {
        [[maybe_unused]] u32 EntryFunctionName;  // offset into the string table
    };

    struct PSV0RuntimeInfo
    {
        [[maybe_unused]] PSVStructVersion version;

        union
        {
            [[maybe_unused]] PSVRuntimeInfo_v1 v1;
            [[maybe_unused]] PSVRuntimeInfo_v2 v2;
            [[maybe_unused]] PSVRuntimeInfo_v3 v3;
            [[maybe_unused]] PSVRuntimeInfo_v4 v4;
            [[maybe_unused]] PSVRuntimeInfo_v4 latest;
        } data;
    };

    struct PSVResourceBindInfo_v1
    {
        [[maybe_unused]] platform::PSVResourceType Type;
        [[maybe_unused]] u32 Space;
        [[maybe_unused]] u32 LowerBound;
        [[maybe_unused]] u32 UpperBound;
    };

    struct PSVResourceBindInfo_v2 : PSVResourceBindInfo_v1
    {
        [[maybe_unused]] platform::PSVResourceKind Kind;
        [[maybe_unused]] u32 Flags;
    };

    struct PSVResourceBindInfo
    {
        [[maybe_unused]] PSVStructVersion version;

        union
        {
            [[maybe_unused]] PSVResourceBindInfo_v1 v1;
            [[maybe_unused]] PSVResourceBindInfo_v2 v2;
            [[maybe_unused]] PSVResourceBindInfo_v2 latest;
        } data;
    };

    struct DXBCHeader
    {
        [[maybe_unused]] char Magic[4];
        [[maybe_unused]] u8 Digest[16];
        [[maybe_unused]] u16 MajorVersion;
        [[maybe_unused]] u16 MinorVersion;
        [[maybe_unused]] u32 FileSize;
        [[maybe_unused]] u32 PartCount;
    };

    struct DXBCPartHeader
    {
        [[maybe_unused]] std::array<char, 4> Name;
        [[maybe_unused]] u32 Size;
    };

    struct PSV0PartHeader
    {
        [[maybe_unused]] DXBCPartHeader Header;
        [[maybe_unused]] u32 RuntimeInfoSize;
    };

    struct PSV0PartData
    {
        [[maybe_unused]] PSV0RuntimeInfo rt_info {.version = PSVStructVersion::notset};
        [[maybe_unused]] cpp::heap_array<PSVResourceBindInfo> resource_bindings;
    };

    PSVStructVersion psv0_get_binding_struct_version(const u32 size)
    {
        ZoneScoped;
        switch (size)
        {
        case sizeof(PSVResourceBindInfo_v1) :
            return PSVStructVersion::v1;
        case sizeof(PSVResourceBindInfo_v2) :
            return PSVStructVersion::v2;
        default :
            break;
        }

        return size > sizeof(PSVResourceBindInfo_v2) ? PSVStructVersion::unrecognized : PSVStructVersion::notset;
    }

    PSVStructVersion psv0_get_info_struct_version(const u32 size)
    {
        ZoneScoped;
        switch (size)
        {
        case sizeof(PSVRuntimeInfo_v1) :
            return PSVStructVersion::v1;
        case sizeof(PSVRuntimeInfo_v2) :
            return PSVStructVersion::v2;
        case sizeof(PSVRuntimeInfo_v3) :
            return PSVStructVersion::v3;
        case sizeof(PSVRuntimeInfo_v4) :
            return PSVStructVersion::v4;
        default :
            break;
        }

        return size > sizeof(PSVRuntimeInfo_v4) ? PSVStructVersion::unrecognized : PSVStructVersion::notset;
    }

    D3D_ROOT_SIGNATURE_VERSION get_root_signature_version(ID3D12Device* device)
    {
        ZoneScoped;
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
    }

    D3D12_SHADER_VISIBILITY get_shader_visibility(const platform::PSVShaderKind shader_type)
    {
        ZoneScoped;
        switch (shader_type)
        {
        case platform::PSVShaderKind::Hull :
            return D3D12_SHADER_VISIBILITY_HULL;
        case platform::PSVShaderKind::Domain :
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case platform::PSVShaderKind::Geometry :
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case platform::PSVShaderKind::Pixel :
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case platform::PSVShaderKind::Amplification :
            return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        case platform::PSVShaderKind::Mesh :
            return D3D12_SHADER_VISIBILITY_MESH;
        case platform::PSVShaderKind::Compute :
            return D3D12_SHADER_VISIBILITY_ALL;
        default :
        case platform::PSVShaderKind::Vertex :
            return D3D12_SHADER_VISIBILITY_VERTEX;
        }
    }

    D3D12_ROOT_PARAMETER_TYPE get_root_parameter_type(const platform::PSVResourceType type)
    {
        ZoneScoped;
        switch (type)
        {
        case platform::PSVResourceType::CBV :
            return D3D12_ROOT_PARAMETER_TYPE_CBV;
        case platform::PSVResourceType::SRVRaw :
        case platform::PSVResourceType::SRVStructured :
            return D3D12_ROOT_PARAMETER_TYPE_SRV;
        case platform::PSVResourceType::UAVRaw :
        case platform::PSVResourceType::UAVStructured :
            return D3D12_ROOT_PARAMETER_TYPE_UAV;
        default :
            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }
    }

    D3D12_DESCRIPTOR_RANGE_TYPE get_descriptor_range_type(const platform::PSVResourceType type)
    {
        ZoneScoped;
        switch (type)
        {
        case platform::PSVResourceType::CBV :
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case platform::PSVResourceType::SRVRaw :
        case platform::PSVResourceType::SRVTyped :
        case platform::PSVResourceType::SRVStructured :
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case platform::PSVResourceType::UAVRaw :
        case platform::PSVResourceType::UAVTyped :
        case platform::PSVResourceType::UAVStructured :
        case platform::PSVResourceType::UAVStructuredWithCounter :
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        default :
        case platform::PSVResourceType::Sampler :
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        }
    }

    cpp::heap_array<D3D12_ROOT_PARAMETER1> collect_root_signature_parameters(const platform::d3d12_shader* shaders,
                                                                             const u32 shaders_count)
    {
        ZoneScoped;
        u32 parameters[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER + 1][4][32] {};
        constexpr u32 kInvalidDedup = ~static_cast<u32>(0);
        constexpr u32 kTotalParams  = COUNT_OF(parameters) * 4 * 32;

        cpp::cx_fill(
            reinterpret_cast<u32*>(parameters), reinterpret_cast<u32*>(parameters) + kTotalParams, kInvalidDedup);

        cpp::heap_array<D3D12_ROOT_PARAMETER1> result;

        u32 descriptor_range_id = 0;
        static D3D12_DESCRIPTOR_RANGE1 s_descriptor_ranges[32];

        u32 size_in_dwords = 0;
        for (u32 i = 0; i < shaders_count; i++)
        {
            auto& shader_meta = shaders[i].dxil_meta;

            for (u32 j = 0; j < shader_meta.resources_count; j++)
            {
                auto& resource  = shader_meta.resources[j];
                const auto type = get_descriptor_range_type(resource.resource_type);

                auto& redirect = parameters[type][resource.space][resource.slot];
                if (redirect != kInvalidDedup)
                {
                    result[redirect].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                    continue;
                }

                redirect   = result.size();
                auto* rptr = &result.emplace_back();

                rptr->ShaderVisibility = get_shader_visibility(shader_meta.shader_type);
                rptr->ParameterType    = resource.size > 0 ? D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE
                                                           : get_root_parameter_type(resource.resource_type);

                switch (rptr->ParameterType)
                {
                case D3D12_ROOT_PARAMETER_TYPE_CBV :
                case D3D12_ROOT_PARAMETER_TYPE_SRV :
                case D3D12_ROOT_PARAMETER_TYPE_UAV :
                {
                    size_in_dwords += 2;
                    rptr->Descriptor.RegisterSpace  = resource.space;
                    rptr->Descriptor.ShaderRegister = resource.slot;
                    break;
                }
                case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE :
                {
                    size_in_dwords += 1;
                    auto* ptr = &s_descriptor_ranges[descriptor_range_id++];

                    ptr->Flags              = resource.size > 0 ? D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
                                                                : D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
                    ptr->RegisterSpace      = resource.space;
                    ptr->BaseShaderRegister = resource.slot;
                    ptr->NumDescriptors     = resource.size > 0 ? resource.size : 1;
                    ptr->OffsetInDescriptorsFromTableStart    = 0;
                    ptr->RangeType                            = type;
                    rptr->DescriptorTable.NumDescriptorRanges = 1;
                    rptr->DescriptorTable.pDescriptorRanges   = ptr;
                    break;
                }
                default :
                    break;
                }
            }
        }

        assert2(size_in_dwords <= 64);
        return result;
    }

    result<platform::com_ptr<ID3D12RootSignature>> create_root_signature(ID3D12Device* device,
                                                                         const platform::d3d12_shader* shaders,
                                                                         const u32 shaders_count)
    {
        ZoneScoped;
        const auto params = collect_root_signature_parameters(shaders, shaders_count);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
        root_signature_desc.Init_1_1(params.size(), params.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        static D3D_ROOT_SIGNATURE_VERSION root_signature_version = get_root_signature_version(device);

        platform::com_ptr<ID3DBlob> err_blob;
        platform::com_ptr<ID3DBlob> root_signature_blob;

        D3D12_RETURN_ON_FAIL(D3DX12SerializeVersionedRootSignature(
            &root_signature_desc, root_signature_version, &root_signature_blob, &err_blob));

        platform::com_ptr<ID3D12RootSignature> root_signature;
        D3D12_RETURN_ON_FAIL(device->CreateRootSignature(0,
                                                         root_signature_blob->GetBufferPointer(),
                                                         root_signature_blob->GetBufferSize(),
                                                         IID_PPV_ARGS(&root_signature)));

        return root_signature;
    }
}

void platform::d3d12_pipeline::bind(ID3D12GraphicsCommandList9* cmd, const rhi::cull_mode cull_mode,
                                    const vec3 depth_bias_params) const
{
    if (this->topology == D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
    {
        cmd->SetPipelineState(this->variants.front().Get());
        cmd->SetComputeRootSignature(this->root_signature.Get());
        return;
    }

    if (this->variants.size() == 1)
    {
        cmd->SetPipelineState(this->variants.front().Get());
    }
    else
    {
        ID3D12PipelineState* pso = nullptr;
        switch (cull_mode)
        {
        case rhi::cull_mode::none :
            pso = this->variants[d3d12_pso_dynstates::cull_mode_none].Get();
            break;
        case rhi::cull_mode::back :
            pso = this->variants[d3d12_pso_dynstates::cull_mode_back].Get();
            break;
        case rhi::cull_mode::front :
            pso = this->variants[d3d12_pso_dynstates::cull_mode_front].Get();
            break;
        default :
            assert2("unsupported cull mode");
            break;
        }

        cmd->SetPipelineState(pso ? pso : this->variants.front().Get());
    }

    if (depth_bias_params != vec3(0.0F))
    {
        cmd->RSSetDepthBias(depth_bias_params.x, depth_bias_params.y, depth_bias_params.z);
    }

    cmd->IASetPrimitiveTopology(this->topology);
    cmd->SetGraphicsRootSignature(this->root_signature.Get());
}

auto platform::d3d12_parse_dxil(const bytes& data) -> result<d3d12_shader_meta>
{
    ZoneScoped;
    u64 cursor = 0;

    auto fcc = [](const char name[4])
    {
        return cpp::stack_string(name, 4);
    };

    auto read = [&]<typename T>(T* dst, const u64 offset, const u32 size = sizeof(T))
    {
        std::memcpy(dst, data.get<u8>() + offset, size);
        cursor = offset + size;
    };

    DXBCHeader header {};
    read(&header, 0);

    assert2(fcc(header.Magic) == "DXBC");
    assert2(header.PartCount > 0);

    cpp::heap_array<u32> offsets(header.PartCount);
    for (u32 i = 0; i < header.PartCount; i++)
    {
        read(&offsets[i], cursor);
    }

    u32 psv0_offset = 0;
    PSV0PartHeader psv0_header {};
    for (u32 i = 0; i < header.PartCount; i++)
    {
        DXBCPartHeader part_header {};
        read(&part_header, offsets[i]);

        if (fcc(part_header.Name.data()) == "PSV0")
        {
            psv0_offset = offsets[i];
            read(&psv0_header, psv0_offset);
        }
    }

    assert2(psv0_offset > 0);

    PSV0PartData psv0_data;
    psv0_data.rt_info.version = psv0_get_info_struct_version(psv0_header.RuntimeInfoSize);
    read(&psv0_data.rt_info.data,
         psv0_offset + sizeof(PSV0PartHeader),
         cpp::min<u32>(sizeof(psv0_data.rt_info.data.latest), psv0_header.RuntimeInfoSize));

    u32 shader_bindings_count = 0;
    read(&shader_bindings_count,
         psv0_offset + sizeof(PSV0PartHeader) + psv0_header.RuntimeInfoSize,
         sizeof(shader_bindings_count));

    if (shader_bindings_count > 0)
    {
        u32 bindings_struct_size = 0;
        read(&bindings_struct_size, cursor);

        const auto binding_version = psv0_get_binding_struct_version(bindings_struct_size);
        for (u32 i = 0; i < shader_bindings_count; i++)
        {
            auto& binding_info   = psv0_data.resource_bindings.emplace_back();
            binding_info.version = binding_version;

            const auto base = cursor;
            read(
                &binding_info.data.latest, base, cpp::min<u32>(sizeof(binding_info.data.latest), bindings_struct_size));

            cursor = base + bindings_struct_size;
        }
    }

    d3d12_shader_meta dxil_meta {
        .threads     = uvec3(psv0_data.rt_info.data.v3.NumThreadsX,
                         psv0_data.rt_info.data.v3.NumThreadsY,
                         psv0_data.rt_info.data.v3.NumThreadsZ),
        .shader_type = psv0_data.rt_info.data.v2.Kind,
    };

    assert2(psv0_data.resource_bindings.size() < COUNT_OF(dxil_meta.resources));
    dxil_meta.resources_count = cpp::min(psv0_data.resource_bindings.size(), COUNT_OF(dxil_meta.resources));
    for (u32 i = 0; i < dxil_meta.resources_count; i++)
    {
        auto& dst = dxil_meta.resources[i];
        auto& src = psv0_data.resource_bindings[i];

        dst.slot  = src.data.v1.LowerBound;
        dst.size  = src.data.v1.UpperBound - src.data.v1.LowerBound;
        dst.space = src.data.v1.Space;

        dst.resource_type = src.data.v1.Type;
        dst.resource_kind = src.data.v2.Kind;
    }

    return dxil_meta;
}

rhi::shader_stage platform::d3d12_rhi_translate_shader_stage(const PSVShaderKind kind)
{
    ZoneScoped;
    switch (kind)
    {
    case PSVShaderKind::Pixel :
        return rhi::shader_stage::fragment;
    case PSVShaderKind::Compute :
        return rhi::shader_stage::compute;
    case PSVShaderKind::Mesh :
        return rhi::shader_stage::mesh;
    case PSVShaderKind::Amplification :
        return rhi::shader_stage::task;
    default :
    case PSVShaderKind::Vertex :
        return rhi::shader_stage::vertex;
    }
}

result<platform::d3d12_shader> platform::d3d12_create_shader(const fs::path& shader_path)
{
    ZoneScoped;
    const auto d3d12_binary = fs::read_file(shader_path.append(".dxl"));
    RESULT_FORWARD_IF_FAILED(d3d12_binary);

    const auto dxil_meta = d3d12_parse_dxil(*d3d12_binary);
    RESULT_FORWARD_IF_FAILED(dxil_meta);

    return platform::d3d12_shader {
        .dxil_bytecode = *d3d12_binary,
        .dxil_meta     = *dxil_meta,
    };
}

void platform::d3d12_destroy_shader(d3d12_shader& shader)
{
    ZoneScoped;
    shader.dxil_bytecode.release();
    memset(&shader.dxil_meta, 0, sizeof(shader.dxil_meta));
}

auto platform::d3d12_create_pipeline_compute(ID3D12Device2* device, const d3d12_shader& shader)
    -> result<d3d12_pipeline>
{
    ZoneScoped;
    const auto rs = create_root_signature(device, &shader, 1);
    RESULT_FORWARD_IF_FAILED(rs);

    CD3DX12_PIPELINE_STATE_STREAM2 desc = {};
    desc.pRootSignature                 = rs->Get();
    desc.CS                             = {
                                    .pShaderBytecode = shader.dxil_bytecode.data(),
                                    .BytecodeLength  = shader.dxil_bytecode.size(),
    };

    const D3D12_PIPELINE_STATE_STREAM_DESC stream_desc {
        .SizeInBytes                   = sizeof(desc),
        .pPipelineStateSubobjectStream = &desc,
    };

    platform::com_ptr<ID3D12PipelineState> pipeline;
    D3D12_RETURN_ON_FAIL(device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&pipeline)));

    cpp::heap_array<platform::com_ptr<ID3D12PipelineState>> variants;
    variants.emplace_back(pipeline);

    return d3d12_pipeline {.root_signature = *rs, .variants = variants, .work_group_size = shader.dxil_meta.threads};
}

auto platform::d3d12_create_pipeline_graphics(ID3D12Device2* device, const d3d12_shader* shaders,
                                              const u32 shaders_count, const rhi::pso_options& options)
    -> result<d3d12_pipeline>
{
    ZoneScoped;
    const auto rs = create_root_signature(device, shaders, shaders_count);
    RESULT_FORWARD_IF_FAILED(rs);

    CD3DX12_PIPELINE_STATE_STREAM2 desc = {};
    desc.pRootSignature                 = rs->Get();

    switch (options.topology)
    {
    case rhi::topology::list_points :
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        break;
    case rhi::topology::list_patches :
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        break;
    case rhi::topology::list_lines :
    case rhi::topology::strip_lines :
    case rhi::topology::strip_lines_adjacent :
    case rhi::topology::list_lines_adjacent :
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        break;
    default :
    case rhi::topology::list_triangles :
    case rhi::topology::strip_triangles :
    case rhi::topology::list_triangles_adjacent :
    case rhi::topology::strip_triangles_adjacent :
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        break;
    }

    desc.DSVFormat = d3d12_format_from_vk(static_cast<VkFormat>(options.depth_format));

    assert2(options.color_attachments_count < COUNT_OF(options.color_formats));
    D3D12_RT_FORMAT_ARRAY rt_format_array = {.NumRenderTargets = options.color_attachments_count};
    for (u32 i = 0; i < options.color_attachments_count; ++i)
    {
        rt_format_array.RTFormats[i] = d3d12_format_from_vk(static_cast<VkFormat>(options.color_formats[i]));
    }

    desc.RTVFormats = rt_format_array;

    CD3DX12_RASTERIZER_DESC raster(D3D12_DEFAULT);
    raster.FrontCounterClockwise = TRUE;

    const bool has_depth = options.depth_format != rhi::image_format::none;
    CD3DX12_DEPTH_STENCIL_DESC1 depth(D3D12_DEFAULT);
    depth.DepthFunc   = d3d12_rhi_parse_comparison_func(options.depth_compare_op);
    depth.DepthEnable = has_depth && options.enable_depth_test;
    depth.DepthWriteMask =
        has_depth && options.enable_depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState = depth;

    if (options.enable_depth_bias)
    {
        desc.Flags = D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS;
    }

    for (u32 i = 0; i < shaders_count; ++i)
    {
        D3D12_SHADER_BYTECODE* dst = nullptr;
        switch (shaders[i].dxil_meta.shader_type)
        {
        case PSVShaderKind::Pixel :
            dst = &desc.PS;
            break;
        case PSVShaderKind::Mesh :
            dst = &desc.MS;
            break;
        case PSVShaderKind::Vertex :
            dst = &desc.VS;
            break;
        case PSVShaderKind::Amplification :
            dst = &desc.AS;
            break;
        default :
            assert2m(false, "unknown shader stage");
            break;
        }

        assert2(dst && "unsupported shader stage, dont do this pls");
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

    auto set_cull_mode = [&](const D3D12_CULL_MODE cull_mode)
    {
        raster.CullMode      = cull_mode;
        desc.RasterizerState = raster;
    };

    cpp::heap_array<platform::com_ptr<ID3D12PipelineState>> variants;
    if (!options.enable_dynstate_cull_mode)
    {
        variants.resize(1);
        set_cull_mode(D3D12_CULL_MODE_BACK);
        D3D12_RETURN_ON_FAIL(device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&variants[0])));
    }
    else
    {
        variants.resize(static_cast<u32>(d3d12_pso_dynstates::COUNT));

        set_cull_mode(D3D12_CULL_MODE_NONE);
        D3D12_RETURN_ON_FAIL(
            device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&variants[d3d12_pso_dynstates::cull_mode_none])));

        set_cull_mode(D3D12_CULL_MODE_BACK);
        D3D12_RETURN_ON_FAIL(
            device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&variants[d3d12_pso_dynstates::cull_mode_back])));

        set_cull_mode(D3D12_CULL_MODE_FRONT);
        D3D12_RETURN_ON_FAIL(
            device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(&variants[d3d12_pso_dynstates::cull_mode_front])));
    }

    return d3d12_pipeline {
        .root_signature = *rs,
        .variants       = variants,
        .topology       = d3d12_rhi_parse_topology(options.topology),
    };
}

void platform::d3d12_destroy_pipeline(d3d12_pipeline& pipeline)
{
    ZoneScoped;
    pipeline.variants.clear();
    pipeline.root_signature.Reset();
}
