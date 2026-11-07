#include <passes/pass_dbg_frustum.hpp>
#include <passes/scoped_label.hpp>
#include <tracy/Tracy.hpp>

app::pass_dbg_frustum::pass_dbg_frustum(const rhi::impl& impl, const app::pso_data& pipelines,
                                        const rhi::context context)
    : m_rhi(impl)
    , m_pipelines(pipelines)
    , m_ctx(context)
{
}

void app::pass_dbg_frustum::draw(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Debug frustum");

    constexpr rhi::barrier_stage_bits kAttachmentStages = rhi::barrier_stage::color_attachment
                                                        | rhi::barrier_stage::early_depth_stencil
                                                        | rhi::barrier_stage::late_depth_stencil;
    constexpr rhi::barrier_access_bits kAttachmentAccess =
        rhi::barrier_access::color_attachment_read | rhi::barrier_access::color_attachment_write
        | rhi::barrier_access::depth_stencil_read | rhi::barrier_access::depth_stencil_write;

    constexpr rhi::global_barrier barrier {
        .before = {.stages = kAttachmentStages, .access = kAttachmentAccess},
        .after  = {.stages = kAttachmentStages, .access = kAttachmentAccess},
    };

    const rhi::barrier_batch barriers {
        .globals = {&barrier, 1}
    };

    m_rhi.cmd_barriers(cmd, barriers);

    const rhi::attachment_state_info color_attachments[] = {
        {.attachment = params.output,
         .load_op    = rhi::resource_load_op::load,
         .store_op   = rhi::resource_store_op::store},
    };

    m_rhi.cmd_set_draw_state(
        cmd,
        {color_attachments, COUNT_OF(color_attachments)},
        {.attachment = params.depth, .load_op = rhi::resource_load_op::load, .store_op = rhi::resource_store_op::store},
        {ivec2(0, 0), params.viewport});

    struct frustum_pc_data
    {
        glm::mat4 renderer_vp;
        glm::vec4 color;
        u32 mode;
    };

    const frustum_pc_data pc_edge {.renderer_vp = params.camera_vp, .color = vec4(1.0, 1.0, 0.0, 1.0), .mode = 0};
    const frustum_pc_data pc_face {.renderer_vp = params.camera_vp, .color = vec4(1.0, 1.0, 0.0, 0.3), .mode = 1};

    const rhi::binding bindings[] = {params.cull_data};

    const auto edge_pso = m_pipelines[pso_id::frustum_debug];
    const auto face_pso = m_pipelines[pso_id::frustum_debug_face];

    m_rhi.cmd_set_cull_mode(cmd, rhi::cull_mode::none);

    m_rhi.cmd_bind_pso(cmd, edge_pso);
    m_rhi.cmd_push_constants(cmd, edge_pso, &pc_edge, sizeof(pc_edge), 0);
    m_rhi.cmd_push_bindings(cmd, edge_pso, std::span {bindings});
    m_rhi.cmd_draw(cmd, 24, 1, 0, 0);

    m_rhi.cmd_bind_pso(cmd, face_pso);
    m_rhi.cmd_push_constants(cmd, face_pso, &pc_face, sizeof(pc_face), 0);
    m_rhi.cmd_push_bindings(cmd, face_pso, std::span {bindings});
    m_rhi.cmd_draw(cmd, 36, 1, 0, 0);

    m_rhi.cmd_clear_draw_state(cmd);
    m_rhi.cmd_barriers(cmd, barriers);
}
