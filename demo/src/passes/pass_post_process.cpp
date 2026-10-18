#include <app/render.hpp>
#include <passes/pass_post_process.hpp>
#include <passes/scoped_label.hpp>

app::pass_post_process::pass_post_process(const rhi::impl& impl, const rhi::context context, const uvec2 display_res)
    : m_ctx(context)
    , m_rhi(impl)
{
    create_resources(display_res);
}

void app::pass_post_process::shutdown()
{
    ZoneScoped;
    destroy_resources();
}

void app::pass_post_process::on_display_size_changed(const uvec2 new_display_size)
{
    ZoneScoped;
    if (m_display_res == new_display_size)
    {
        return;
    }

    destroy_resources();
    create_resources(new_display_size);
}

void app::pass_post_process::draw(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Post process");

    const rhi::image_barrier transition_barriers[] {
        app::make_image_barrier(this->output, rhi::image_layout::common, rhi::image_aspect::color),
    };
    m_rhi.cmd_barriers(cmd,
                       rhi::barrier_batch {
                           .images = std::span {transition_barriers},
                       });

    const rhi::binding tonemap_pass_bindings[] = {
        rhi::binding(params.color),
        rhi::binding(output),
    };

    const rhi::pipeline& tonemap_pass = params.pipeline;

    m_rhi.cmd_bind_pso(cmd, tonemap_pass);
    m_rhi.cmd_push_bindings(cmd, tonemap_pass, std::span {tonemap_pass_bindings});
    m_rhi.cmd_push_constants(cmd, tonemap_pass, &m_display_res, sizeof(m_display_res), 0);

    m_rhi.cmd_dispatch(cmd, tonemap_pass, uvec3(m_display_res, 1));

    constexpr rhi::global_barrier barrier {
        .before = {.stages = rhi::barrier_stage::compute_shader,   .access = rhi::barrier_access::storage_write      },
        .after  = {.stages = rhi::barrier_stage::color_attachment,
                   .access = rhi::barrier_access::color_attachment_read | rhi::barrier_access::color_attachment_write},
    };

    const rhi::barrier_batch barriers {
        .globals = {&barrier, 1}
    };

    m_rhi.cmd_barriers(cmd, barriers);
}

void app::pass_post_process::destroy_resources()
{
    ZoneScoped;
    m_rhi.destroy_image(m_ctx, output);
}

void app::pass_post_process::create_resources(const uvec2 display_res)
{
    ZoneScoped;
    m_display_res = display_res;
    output        = app::create_color_image(
        m_rhi, display_res, rhi::image_format::r8g8b8a8un, m_ctx, rhi::image_usage::transfer_src);
}
