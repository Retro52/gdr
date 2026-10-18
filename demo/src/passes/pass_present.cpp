#include <app/render.hpp>
#include <passes/pass_present.hpp>
#include <passes/scoped_label.hpp>
#include <tracy/Tracy.hpp>

app::pass_present::pass_present(const rhi::impl& impl, const rhi::context context)
    : m_rhi(impl)
    , m_ctx(context)
{
    ZoneScoped;
}

void app::pass_present::present(const rhi::command_buffer cmd, const rhi::swapchain sc, const draw_params& params) const
{
    ZoneScoped;

    {
        SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Present");
        const rhi::image_barrier transition_barriers[] {
            app::make_image_barrier(params.output, rhi::image_layout::common),
        };

        constexpr rhi::barrier_stage_bits kAttachmentStages = rhi::barrier_stage::color_attachment
                                                            | rhi::barrier_stage::early_depth_stencil
                                                            | rhi::barrier_stage::late_depth_stencil;
        constexpr rhi::barrier_access_bits kAttachmentAccess =
            rhi::barrier_access::color_attachment_read | rhi::barrier_access::color_attachment_write
            | rhi::barrier_access::depth_stencil_read | rhi::barrier_access::depth_stencil_write;

        constexpr rhi::global_barrier global_barrier {
            .before = {.stages = kAttachmentStages,        .access = kAttachmentAccess             },
            .after  = {.stages = rhi::barrier_stage::copy, .access = rhi::barrier_access::copy_read},
        };

        m_rhi.cmd_barriers(cmd,
                           rhi::barrier_batch {
                               .images  = std::span {transition_barriers},
                               .globals = std::span {&global_barrier, 1},
        });

        m_rhi.cmd_image_blit(cmd,
                             params.color,
                             params.output,
                             rhi::blit_image_info {
                                 .src_extent = ivec3(params.color_res, 1),
                                 .dst_extent = ivec3(params.display_res, 1),
                                 .aspects    = rhi::image_aspect::color,
                             });
        const rhi::image_barrier present_barriers[] {
            app::make_image_barrier(params.output, rhi::image_layout::present),
        };
        m_rhi.cmd_barriers(cmd,
                           rhi::barrier_batch {
                               .images = std::span {present_barriers},
                           });
    }

    m_rhi.cmd_end_recording(cmd);
    m_rhi.present(
        cmd, sc, *m_rhi.query_queue(m_ctx, rhi::queue_kind::gfx), *m_rhi.query_queue(m_ctx, rhi::queue_kind::present));
}
