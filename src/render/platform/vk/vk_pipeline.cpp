#include <cpp/hash/hashed_string.hpp>
#include <fs/fs.hpp>
#include <render/platform/vk/vk_pipeline.hpp>

#define SPV_ENABLE_UTILITY_CODE
#include <spirv-headers/spirv.h>
#include <Tracy/Tracy.hpp>

#include <iostream>

using namespace render;

namespace
{
    std::string parse_spv_string(const u32* raw)
    {
        std::string result;
        const char* stream = reinterpret_cast<const char*>(raw);
        while (*stream)
        {
            result += *stream++;
        }

        return result;
    }

    VkShaderStageFlagBits get_shader_stage(u32 word)
    {
        switch (static_cast<SpvExecutionModel>(word))
        {
        case SpvExecutionModelFragment :
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SpvExecutionModelVertex :
            return VK_SHADER_STAGE_VERTEX_BIT;
        case SpvExecutionModelMeshEXT :
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        case SpvExecutionModelTaskEXT :
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case SpvExecutionModelGLCompute :
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default :
            break;
        }

        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
}

result<shader> shader::load(const vk_renderer& renderer, const fs::path& path)
{
    ZoneScoped;
    const auto binary = fs::read_file(path);
    const VkShaderModuleCreateInfo module_create_info {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size(),
        .pCode    = binary.get<u32>(),
    };

    VkShaderModule shader_module;
    VK_RETURN_ON_FAIL(vkCreateShaderModule(renderer.get_context().device, &module_create_info, nullptr, &shader_module))

    return shader {.module = shader_module, .meta = parse_spirv(binary)};
}

shader::shader_meta shader::parse_spirv(const bytes& spv)
{
    ZoneScoped;

    struct spv_id
    {
        u32 name;                       // numerical name
        SpvStorageClass storage_class;  // variable type?
    };

    shader_meta result {};
    assert(!spv.empty() && spv.size() % 4 == 0 && "SPV shall not be empty, and contain a stream of u32 packed data");

    // As per Khronos spec:
    // 0 - magic number
    // 1 - packed version
    // 2 - one more magic number from a compile tool
    // 3 - ID count
    // 4 - reserved
    // 5 - first word of an instruction stream

    const u32* inst = static_cast<const u32*>(spv.data());
    assert(inst[0] == SpvMagicNumber);

    const u32 id_count = inst[3];
    std::vector<spv_id> spv_ids(id_count);

    // 5 = first actual instruction offset as per specification
    inst += 5;
    std::cerr << "=================================================\n";

    // TODO: an actual parsing
    // TODO: parse shader binding layout
    // TODO: parse shader push range
    const u32* end = static_cast<const u32*>(spv.end());
    while (inst < end)
    {
        u32 word = inst[0];

        u16 op_code       = static_cast<u16>(word);
        u16 op_word_count = static_cast<u16>(word >> 16);

        std::cerr << "[" << inst - spv.get<u32>() << "/" << spv.length<u32>() << "] "
                  << "st: " << SpvOpToString(SpvOp(op_code)) << "; "
                  << "wc: " << op_word_count << "; ";

        switch (op_code)
        {
        case SpvOpMemoryModel :
            std::cerr << "am: " << SpvAddressingModelToString(SpvAddressingModel(inst[1])) << "; ";
            std::cerr << "mm: " << SpvMemoryModelToString(SpvMemoryModel(inst[2])) << "; ";
            break;
        case SpvOpExecutionMode :
            std::cerr << "ep: " << inst[1] << "; ";
            std::cerr << "md: " << SpvExecutionModeToString(SpvExecutionMode(inst[2])) << "; ";
            break;
        case SpvOpName :
            std::cerr << "id: " << inst[1] << "; ";
            std::cerr << "nm: " << parse_spv_string(inst + 2) << "; ";
            break;
        case SpvOpMemberName :
            std::cerr << "tp: " << inst[1] << "; ";
            std::cerr << "mb: " << inst[2] << "; ";
            std::cerr << "nm: " << parse_spv_string(inst + 3) << "; ";
            break;
        case SpvOpExtInstImport:
            // <extinst-id> OpExtInstImport "name-of-extended-instruction-set"
            // extinst-id - id of the extensions set
            // OpExtInst <extinst-id> instruction-number operand0, operand1, ...
            std::cerr << "ed: " << inst[1] << "; ";
            std::cerr << "nm: " << parse_spv_string(inst + 2) << "; ";
            break;
        case SpvOpSourceExtension :
            std::cerr << "et: " << parse_spv_string(inst + 1) << "; ";
            break;
        case SpvOpEntryPoint :
            result.stage = get_shader_stage(inst[1]);
            std::cerr << "em: " << SpvExecutionModelToString(SpvExecutionModel(inst[1])) << "; ";
            std::cerr << "ep: " << inst[2] << "; ";
            std::cerr << "nm: " << parse_spv_string(inst + 3) << "; ";
            break;
        default :
            std::cerr << "words: [";
            for (u32 id = 1; id < op_word_count; ++id)
            {
                std::cerr << inst[id] << (id == op_word_count - 1 ? "" : ", ");
            }
            std::cerr << "]; ";
            break;
        }

        inst += op_word_count;
        std::cerr << "\n";
    }
    std::cerr << "=================================================\n";

    return result;
}

result<pipeline> pipeline::create_graphics(const vk_renderer& renderer, const shader* shaders, u32 shaders_count,
                                           const VkPushConstantRange* push_constants, u32 pc_count,
                                           VkVertexInputBindingDescription vertex_bind,
                                           const VkVertexInputAttributeDescription* vertex_attr, u32 vertex_attr_count)
{
    ZoneScoped;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos(shaders_count);
    for (u32 i = 0; i < shaders_count; ++i)
    {
        shader_stage_create_infos[i] = {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = shaders[i].meta.stage,
            .module = shaders[i].module,
            .pName  = "main",
        };
    }

    constexpr VkDynamicState dynamic_state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    constexpr u32 dynamic_state_count        = COUNT_OF(dynamic_state);

    const VkPipelineDynamicStateCreateInfo dynamic_state_create_info {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = dynamic_state_count,
        .pDynamicStates    = dynamic_state,
    };

    const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &vertex_bind,
        .vertexAttributeDescriptionCount = vertex_attr_count,
        .pVertexAttributeDescriptions    = vertex_attr,
    };

    const VkPipelineInputAssemblyStateCreateInfo assembly_state_create_info {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const auto scissor  = renderer.get_scissor();
    const auto viewport = renderer.get_viewport();

    const VkPipelineViewportStateCreateInfo viewport_state_create_info {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 1.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState color_blend_attachment_state {
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blend_state_create_info {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment_state,
        .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    const VkPipelineLayoutCreateInfo pipeline_layout_create_info {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 0,
        .pSetLayouts            = nullptr,
        .pushConstantRangeCount = pc_count,
        .pPushConstantRanges    = push_constants,
    };

    VkPipelineLayout vk_pipeline_layout;
    VK_RETURN_ON_FAIL(vkCreatePipelineLayout(
        renderer.get_context().device, &pipeline_layout_create_info, nullptr, &vk_pipeline_layout));

    const VkPipelineRenderingCreateInfo pipeline_rendering_create_info {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &renderer.get_swapchain().surface_format.format,
        .depthAttachmentFormat   = renderer.get_swapchain().depth_format,
    };

    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,
    };

    const VkGraphicsPipelineCreateInfo pipeline_create_info {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &pipeline_rendering_create_info,
        .stageCount          = 2,
        .pStages             = shader_stage_create_infos.data(),
        .pVertexInputState   = &vertex_input_state_create_info,
        .pInputAssemblyState = &assembly_state_create_info,
        .pViewportState      = &viewport_state_create_info,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depth_stencil_state_create_info,
        .pColorBlendState    = &color_blend_state_create_info,
        .pDynamicState       = &dynamic_state_create_info,
        .layout              = vk_pipeline_layout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    VkPipeline vk_pipeline;
    VK_RETURN_ON_FAIL(vkCreateGraphicsPipelines(
        renderer.get_context().device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vk_pipeline));

    return pipeline {renderer.get_context().device, vk_pipeline, vk_pipeline_layout, VK_PIPELINE_BIND_POINT_GRAPHICS};
}

void pipeline::bind(VkCommandBuffer command_buffer) const
{
    vkCmdBindPipeline(command_buffer, m_pipeline_bind_point, m_pipeline);
}

void pipeline::push_constant(VkCommandBuffer command_buffer, u32 size, const void* data) const
{
    vkCmdPushConstants(command_buffer, m_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, size, data);
}

pipeline::pipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                   VkPipelineBindPoint pipeline_bind_point)
    : m_device(device)
    , m_pipeline(pipeline)
    , m_pipeline_layout(pipeline_layout)
    , m_pipeline_bind_point(pipeline_bind_point)
{
}
