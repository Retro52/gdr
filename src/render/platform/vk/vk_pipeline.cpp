#include <fs/fs.hpp>
#include <assert2.hpp>
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

    VkDescriptorType get_descriptor_type(u32 word)
    {
        switch (static_cast<SpvOp>(word))
        {
        case SpvOpTypeStruct :
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SpvOpTypeImage :
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SpvOpTypeSampler :
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SpvOpTypeSampledImage :
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SpvOpTypeAccelerationStructureKHR :
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default :
            break;
        }

        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
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

    VkResult create_update_template(VkDevice device, VkPipelineBindPoint bind_point, VkPipelineLayout layout,
                                    const vk_shader* shaders, u32 shaders_count,
                                    VkDescriptorUpdateTemplate* update_template)
    {
        u32 entries_count = 0;
        VkDescriptorUpdateTemplateEntry entries[COUNT_OF(vk_shader::shader_meta::bindings)] {};

        for (u32 i = 0; i < shaders_count; ++i)
        {
            const auto& shader_meta = shaders[i].meta;
            for (u32 j = 0; j < shader_meta.bindings_count; ++j)
            {
                if (shader_meta.bindings[j] != VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    entries_count = std::max(entries_count, j + 1);
                    entries[j]    = {
                           .dstBinding      = j,
                           .descriptorCount = 1,
                           .descriptorType  = shader_meta.bindings[j],
                           .offset          = sizeof(vk_descriptor_info) * j,
                           .stride          = sizeof(vk_descriptor_info),
                    };
                }
            }
        }

        if (entries_count == 0)
        {
            *update_template = VK_NULL_HANDLE;
            return VK_SUCCESS;
        }

        VkDescriptorUpdateTemplateCreateInfo create_info = {
            .sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
            .descriptorUpdateEntryCount = entries_count,
            .pDescriptorUpdateEntries   = entries,
            .templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR,
            .pipelineBindPoint          = bind_point,
            .pipelineLayout             = layout,
        };

        return vkCreateDescriptorUpdateTemplate(device, &create_info, nullptr, update_template);
    }

    VkPushConstantRange parse_push_constant_range(const vk_shader* shaders, u32 shaders_count)
    {
        VkPushConstantRange pc_range {};

        for (u32 i = 0; i < shaders_count; ++i)
        {
            const auto& shader_meta = shaders[i].meta;
            if (shader_meta.push_constant_struct_size > 0)
            {
                pc_range.stageFlags |= shader_meta.stage;
                pc_range.size = std::max(pc_range.size, shader_meta.push_constant_struct_size);
            }
        }

        return pc_range;
    }

    VkResult create_pipeline_layout(VkDevice device, const vk_shader* shaders, u32 shaders_count,
                                    const VkPushConstantRange& push_constant_range, VkPipelineLayout* layout)
    {
        u32 entries_count = 0;
        VkDescriptorSetLayoutBinding entries[COUNT_OF(vk_shader::shader_meta::bindings)] {};

        for (u32 i = 0; i < shaders_count; ++i)
        {
            const auto& shader_meta = shaders[i].meta;
            for (u32 j = 0; j < shader_meta.bindings_count; ++j)
            {
                if (shader_meta.bindings[j] != VK_DESCRIPTOR_TYPE_MAX_ENUM)
                {
                    entries_count = std::max(entries_count, j + 1);

                    entries[j].binding         = j;
                    entries[j].descriptorType  = shader_meta.bindings[j];
                    entries[j].descriptorCount = 1;
                    entries[j].stageFlags |= shader_meta.stage;
                }
            }
        }

        const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = entries_count,
            .pBindings    = entries};

        VkDescriptorSetLayout desc_set_layout;
        VK_ASSERT_ON_FAIL(vkCreateDescriptorSetLayout(device, &desc_set_layout_info, nullptr, &desc_set_layout));

        const u32 push_constant_count                   = push_constant_range.size > 0 ? 1 : 0;
        const VkPushConstantRange* push_constant_ranges = push_constant_count > 0 ? &push_constant_range : nullptr;

        const VkPipelineLayoutCreateInfo pipeline_layout_create_info {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount         = 1,
            .pSetLayouts            = &desc_set_layout,
            .pushConstantRangeCount = push_constant_count,
            .pPushConstantRanges    = push_constant_ranges,
        };

        // FIXME: memory leak
        // vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
        return vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, layout);
    }
}

result<vk_shader> vk_shader::load(const vk_renderer& renderer, const fs::path& path)
{
    ZoneScoped;
    const auto binary = *fs::read_file(path);
    const VkShaderModuleCreateInfo module_create_info {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = binary.size(),
        .pCode    = binary.get<u32>(),
    };

    VkShaderModule shader_module;
    VK_RETURN_ON_FAIL(vkCreateShaderModule(renderer.get_context().device, &module_create_info, nullptr, &shader_module))

    return vk_shader {.module = shader_module, .meta = parse_spirv(binary)};
}

vk_shader::shader_meta vk_shader::parse_spirv(const bytes& spv)
{
    ZoneScoped;

    struct spv_id
    {
        u32 name {0};                                        // numerical name
        u32 size {0};                                        // raw size
        SpvOp op_code {SpvOpNop};                            // associated op type
        SpvStorageClass storage_class {SpvStorageClassMax};  // variable storage class
        u32 constant {0};                                    // constant value
        spv_id* type {nullptr};                              // type reference
        DEBUG_ONLY(std::string readable_name);               // parsed from debug info
    };

    struct spv_struct
    {
        spv_id* self {nullptr};         // pointer into a spv_ids array declaring instruction (id)
        std::vector<spv_id*> children;  // associated members
    };

    struct spv_push_desc
    {
        spv_id* variable;
        u32 binding;
        u32 set;
    };

    spv_id* push_constant_variable = nullptr;

    std::unordered_map<u32, spv_struct> structs;
    std::unordered_map<u32, spv_push_desc> push_descriptors;

    auto save_if_push_constant = [&](u32 storage_class, spv_id* self)
    {
        if (static_cast<SpvStorageClass>(storage_class) == SpvStorageClassPushConstant)
        {
            assert2(!push_constant_variable);
            push_constant_variable = self;
        }
    };

    auto get_spv_struct_size_bytes = [](const spv_struct* target)
    {
        u32 size = 0;
        for (const auto& member : target->children)
        {
            size += member ? member->size : 0;
        }

        assert2(size % 8 == 0);
        return size / 8;
    };

    auto get_spv_id_root = [](spv_id* id)
    {
        while (id->type)
        {
            id = id->type;
        }

        return id;
    };

    shader_meta result {};
    assert2(!spv.empty() && spv.size() % 4 == 0 && "SPV shall not be empty, and contain a stream of u32 packed data");

    // As per Khronos spec:
    // 0 - magic number
    // 1 - packed version
    // 2 - one more magic number from a compile tool
    // 3 - ID count
    // 4 - reserved
    // 5 - first word of an instruction stream

    const u32* inst = static_cast<const u32*>(spv.data());
    assert2(inst[0] == SpvMagicNumber);

    const u32 id_count = inst[3];
    std::vector<spv_id> spv_ids(id_count);

    // 5 = first actual instruction offset as per specification
    inst += 5;

    const u32* end = static_cast<const u32*>(spv.end());
    while (inst < end)
    {
        u32 word = inst[0];

        u16 op_code       = static_cast<u16>(word);
        u16 op_word_count = static_cast<u16>(word >> 16);

        switch (op_code)
        {
        case SpvOpDecorate :
        {
            switch (static_cast<SpvDecoration>(inst[2]))
            {
            case SpvDecorationBinding :
                push_descriptors[inst[1]].binding = inst[3];
                break;
            case SpvDecorationDescriptorSet :
                push_descriptors[inst[1]].set      = inst[3];
                push_descriptors[inst[1]].variable = &spv_ids[inst[1]];
                break;
            default :
                break;
            }
            break;
        }
        case SpvOpName :
            DEBUG_ONLY(spv_ids[inst[1]].readable_name = parse_spv_string(inst + 2));
            break;
        case SpvOpEntryPoint :
            result.stage = get_shader_stage(inst[1]);
            break;
        case SpvOpTypeBool :
            spv_ids[inst[1]].name    = inst[1];
            spv_ids[inst[1]].size    = 8;
            spv_ids[inst[1]].op_code = static_cast<SpvOp>(op_code);
            break;
        case SpvOpTypeInt :
        case SpvOpTypeFloat :
            spv_ids[inst[1]].name    = inst[1];
            spv_ids[inst[1]].size    = static_cast<u32>(inst[2]);
            spv_ids[inst[1]].op_code = static_cast<SpvOp>(op_code);
            break;
        case SpvOpTypeArray :
            assert2(spv_ids[inst[3]].name == inst[3]);
            assert2(spv_ids[inst[3]].constant > 0);
            spv_ids[inst[1]].name    = inst[1];
            spv_ids[inst[1]].size    = spv_ids[inst[2]].size * spv_ids[inst[3]].constant;
            spv_ids[inst[1]].op_code = static_cast<SpvOp>(op_code);
            break;
        case SpvOpTypeVector :
        case SpvOpTypeMatrix :
            spv_ids[inst[1]].name    = inst[1];
            spv_ids[inst[1]].size    = spv_ids[inst[2]].size * static_cast<u32>(inst[3]);
            spv_ids[inst[1]].op_code = static_cast<SpvOp>(op_code);
            break;
        case SpvOpTypeStruct :
            structs[inst[1]].self = &spv_ids[inst[1]];
            for (u32 i = 2; i < op_word_count; i++)
            {
                structs[inst[1]].children.push_back(&spv_ids[inst[i]]);
            }
        case SpvOpTypeImage :
        case SpvOpTypeSampler :
        case SpvOpTypeSampledImage :
        case SpvOpTypeAccelerationStructureKHR :
            spv_ids[inst[1]].name    = inst[1];
            spv_ids[inst[1]].op_code = static_cast<SpvOp>(op_code);
            break;
        case SpvOpTypePointer :
            spv_ids[inst[1]].name          = inst[1];
            spv_ids[inst[1]].type          = &spv_ids[inst[3]];
            spv_ids[inst[1]].op_code       = static_cast<SpvOp>(op_code);
            spv_ids[inst[1]].storage_class = static_cast<SpvStorageClass>(inst[2]);
            break;
        case SpvOpVariable :
            spv_ids[inst[2]].name          = inst[2];
            spv_ids[inst[2]].type          = &spv_ids[inst[1]];
            spv_ids[inst[2]].op_code       = static_cast<SpvOp>(op_code);
            spv_ids[inst[2]].storage_class = static_cast<SpvStorageClass>(inst[3]);
            save_if_push_constant(inst[3], &spv_ids[inst[2]]);
            break;
        case SpvOpConstant :
            spv_ids[inst[2]].name     = inst[2];
            spv_ids[inst[2]].constant = inst[3];
            spv_ids[inst[2]].type     = &spv_ids[inst[1]];
            spv_ids[inst[2]].op_code  = static_cast<SpvOp>(op_code);
            break;
        default :
            break;
        }

        inst += op_word_count;
    }

    for (const auto& [_, push_desc] : push_descriptors)
    {
        assert2(push_desc.binding < COUNT_OF(result.bindings) && "binding id too high?");

        // find the root type declaration
        const spv_id* declaration = get_spv_id_root(push_desc.variable);

        result.bindings_count              = std::max(result.bindings_count, push_desc.binding + 1);
        result.bindings[push_desc.binding] = get_descriptor_type(declaration->op_code);
    }

    if (push_constant_variable)
    {
        const spv_id* declaration        = get_spv_id_root(push_constant_variable);
        result.push_constant_struct_size = declaration->op_code == SpvOpTypeStruct
                                             ? get_spv_struct_size_bytes(&structs[declaration->name])
                                             : declaration->size;
    }

    if (result.push_constant_struct_size > 128)
    {
        std::cerr << "WARNING: Parsed PushConstants range is over 128 bytes. Parsed size: "
                  << result.push_constant_struct_size << std::endl;
    }

    return result;
}

result<vk_pipeline> vk_pipeline::create_compute(const vk_renderer& renderer, const vk_shader& shader)
{
    assert2(shader.meta.stage == VK_SHADER_STAGE_COMPUTE_BIT);

    const auto pc_range = parse_push_constant_range(&shader, 1);

    VkPipelineLayout pipeline_layout;
    VK_RETURN_ON_FAIL(create_pipeline_layout(renderer.get_context().device, &shader, 1, pc_range, &pipeline_layout));

    VkDescriptorUpdateTemplate update_template;
    VK_RETURN_ON_FAIL(create_update_template(
        renderer.get_context().device, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, &shader, 1, &update_template));

    const VkPipelineShaderStageCreateInfo shader_stage_info = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = shader.meta.stage,
        .module = shader.module,
        .pName  = "main",
    };

    const VkComputePipelineCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = shader_stage_info, .layout = pipeline_layout};

    VkPipeline pipeline;
    VK_RETURN_ON_FAIL(
        vkCreateComputePipelines(renderer.get_context().device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));

    return vk_pipeline {pipeline,
                        pipeline_layout,
                        update_template,
                        VK_PIPELINE_BIND_POINT_COMPUTE,
                        VK_SHADER_STAGE_COMPUTE_BIT,
                        DEBUG_ONLY(pc_range.size)};
}

result<vk_pipeline> vk_pipeline::create_graphics(const vk_renderer& renderer, const vk_shader* shaders,
                                                 u32 shaders_count, VkPrimitiveTopology topology)
{
    ZoneScoped;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos(shaders_count);
    for (u32 i = 0; i < shaders_count; ++i)
    {
        assert2(shaders[i].meta.stage != VK_SHADER_STAGE_COMPUTE_BIT);
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
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    const VkPipelineInputAssemblyStateCreateInfo assembly_state_create_info {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = topology,
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
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

    const VkPipelineRenderingCreateInfo pipeline_rendering_create_info {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext                   = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &renderer.get_swapchain().surface_format.format,
        .depthAttachmentFormat   = renderer.get_swapchain().depth_format,
    };

    const VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext            = nullptr,
        .depthTestEnable  = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp   = VK_COMPARE_OP_GREATER,
        .minDepthBounds   = 0.0f,
        .maxDepthBounds   = 1.0f,
    };

    VkPipelineLayout pipeline_layout;
    VkPushConstantRange push_constant_range = parse_push_constant_range(shaders, shaders_count);
    create_pipeline_layout(
        renderer.get_context().device, shaders, shaders_count, push_constant_range, &pipeline_layout);

    const VkGraphicsPipelineCreateInfo pipeline_create_info {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &pipeline_rendering_create_info,
        .stageCount          = shaders_count,
        .pStages             = shader_stage_create_infos.data(),
        .pVertexInputState   = &vertex_input_state_create_info,
        .pInputAssemblyState = &assembly_state_create_info,
        .pViewportState      = &viewport_state_create_info,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depth_stencil_state_create_info,
        .pColorBlendState    = &color_blend_state_create_info,
        .pDynamicState       = &dynamic_state_create_info,
        .layout              = pipeline_layout,
        .renderPass          = VK_NULL_HANDLE,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
    };

    VkPipeline vk_handle;
    VK_RETURN_ON_FAIL(vkCreateGraphicsPipelines(
        renderer.get_context().device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &vk_handle));

    VkDescriptorUpdateTemplate update_template;
    VK_RETURN_ON_FAIL(create_update_template(renderer.get_context().device,
                                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                                             pipeline_layout,
                                             shaders,
                                             shaders_count,
                                             &update_template));

    return vk_pipeline {vk_handle,
                        pipeline_layout,
                        update_template,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        push_constant_range.stageFlags,
                        DEBUG_ONLY(push_constant_range.size)};
}

void vk_pipeline::bind(VkCommandBuffer command_buffer) const
{
    vkCmdBindPipeline(command_buffer, m_pipeline_bind_point, m_pipeline);
}

void vk_pipeline::push_constant(VkCommandBuffer command_buffer, u32 size, const void* data) const
{
    DEBUG_ONLY(assert2(m_push_constants_max_size >= size));
    vkCmdPushConstants(command_buffer, m_pipeline_layout, m_push_constant_stages, 0, size, data);
}

void vk_pipeline::push_descriptor_set(VkCommandBuffer command_buffer, const vk_descriptor_info* updates) const
{
    vkCmdPushDescriptorSetWithTemplateKHR(command_buffer, m_descriptor_update_template, m_pipeline_layout, 0, updates);
}
