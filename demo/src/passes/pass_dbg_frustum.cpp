#include <passes/pass_dbg_frustum.hpp>
#include <passes/scoped_label.hpp>
#include <tracy/Tracy.hpp>

app::pass_dbg_frustum::pass_dbg_frustum(const rhi::impl& impl, const rhi::context context, const rhi::pipeline pipeline)
    : m_rhi(impl)
    , m_ctx(context)
    , m_pipeline(pipeline)
{
}

void app::pass_dbg_frustum::draw(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Debug frustum pass");

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

    const struct frustum_pc_data
    {
        glm::mat4 renderer_vp;
    } pc {.renderer_vp = params.camera_vp};

    const rhi::binding bindings[] = {params.cull_data};

    m_rhi.cmd_bind_pso(cmd, m_pipeline);
    m_rhi.cmd_push_constants(cmd, m_pipeline, &pc, sizeof(pc), 0);
    m_rhi.cmd_push_bindings(cmd, m_pipeline, std::span {bindings});
    m_rhi.cmd_draw(cmd, 24, 1, 0, 0);

    m_rhi.cmd_clear_draw_state(cmd);
    m_rhi.cmd_barriers(cmd, barriers);
}
