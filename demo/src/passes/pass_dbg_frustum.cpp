#include <app/render.hpp>
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

    const rhi::image_barrier pre_barriers[] {
        app::make_image_barrier(
            params.output.get_image(), rhi::image_layout::common, rhi::image_layout::render_target_color),
        app::make_image_barrier(params.depth.get_image(),
                                rhi::image_layout::common,
                                rhi::image_layout::render_target_depth_stencil,
                                rhi::image_aspect::depth),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span {pre_barriers}});

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

    const rhi::image_barrier post_barriers[] {
        app::make_image_barrier(
            params.output.get_image(), rhi::image_layout::render_target_color, rhi::image_layout::common),
        app::make_image_barrier(params.depth.get_image(),
                                rhi::image_layout::render_target_depth_stencil,
                                rhi::image_layout::common,
                                rhi::image_aspect::depth),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span {post_barriers}});
}
