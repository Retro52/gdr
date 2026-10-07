#include <fs/fs.hpp>
#include <render/platform/d3d12/d3d12_pipeline.hpp>

result<render::d3d12_shader> render::d3d12_create_shader(const fs::path& shader_path)
{
    ZoneScoped;
    const auto spv_binary = fs::read_file(shader_path);
    RESULT_FORWARD_IF_FAILED(spv_binary);

    const auto d3d12_binary = fs::read_file(shader_path.filename().append(".dxl"));
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

auto render::d3d12_create_pipeline_compute(ID3D12Device* device, const d3d12_shader& shader, bool debug,
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
    return error("not implemented");
}

auto render::d3d12_create_pipeline_graphics(ID3D12Device* device, const d3d12_shader* shaders, u32 shaders_count,
                                            bool debug, VkFormat default_color_format, VkFormat default_depth_format,
                                            const d3d12_descriptor_set* desc_set, u32 desc_set_count,
                                            const nlohmann::json& options) -> result<d3d12_pipeline>
{
    return error("not implemented");
}

void render::d3d12_destroy_pipeline(const d3d12_context& context, d3d12_shader& shader)
{
}
