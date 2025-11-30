#include <render/platform/vk/vk_pipeline.hpp>

using namespace render;

result<pipeline> pipeline::create_graphics(const vk_renderer& renderer, VkShaderModule vertex, VkShaderModule fragment,
                                           const VkPushConstantRange* push_constants, u32 pc_count,
                                           VkVertexInputBindingDescription vertex_bind,
                                           const VkVertexInputAttributeDescription* vertex_attr, u32 vertex_attr_count)
{
    const VkPipelineShaderStageCreateInfo vert_shader_stage_info {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex,
        .pName  = "main"};

    const VkPipelineShaderStageCreateInfo frag_shader_stage_info {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment,
        .pName  = "main"};

    constexpr VkDynamicState dynamic_state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    constexpr u32 dynamic_state_count        = DYE_ARR_SIZE(dynamic_state);

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

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
        .pStages             = shader_stages,
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
