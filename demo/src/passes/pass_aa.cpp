#include <app/render.hpp>
#include <passes/pass_aa.hpp>
#include <passes/scoped_label.hpp>

app::pass_aa::pass_aa(const rhi::impl& impl, const app::pso_data& pipelines, const rhi::context context,
                      const uvec2 render_res, const uvec2 display_res)
    : m_ctx(context)
    , m_rhi(impl)
    , m_pipelines(pipelines)
{
    ZoneScoped;
    create_resources(render_res, display_res);
    fxaa_color_sampler = *m_rhi.create_sampler(m_ctx, {.mipmap_mode = rhi::sampler_mipmap_mode::nearest});
    fxaa_depth_sampler = *m_rhi.create_sampler(m_ctx,
                                               {
                                                   .filter       = rhi::sampler_filter::nearest,
                                                   .mipmap_mode  = rhi::sampler_mipmap_mode::nearest,
                                                   .address_mode = rhi::sampler_address_mode::clamp_to_border,
                                               });
}

void app::pass_aa::shutdown()
{
    ZoneScoped;
    destroy_resources();
    m_rhi.destroy_sampler(m_ctx, fxaa_color_sampler);
    m_rhi.destroy_sampler(m_ctx, fxaa_depth_sampler);
}

void app::pass_aa::next_jitter()
{
    m_jitter_offset++;
}

vec2 app::pass_aa::get_frame_jitter() const
{
    return *m_rhi.ffx_upscale_query_jitter(ffx_context, m_render_res.x, m_display_res.x, m_jitter_offset);
}

glm::mat4 app::pass_aa::get_jittered_matrix(const glm::mat4& proj) const
{
    if (this->backend != aa_backend::fsr3)
    {
        return proj;
    }

    const auto resized_jitter = get_frame_jitter() * vec2(2.0F, -2.0F) / vec2(m_render_res);
    return glm::translate(glm::mat4(1.0f), vec3(resized_jitter, 0.0f)) * proj;
}

void app::pass_aa::draw(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "AA pass");

    const rhi::image_barrier pre_barriers[] = {
        app::make_image_barrier(output, rhi::image_layout::discard, rhi::image_layout::storage),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span(pre_barriers)});

    switch (backend)
    {
    case aa_backend::blit :
        run_blit(cmd, params);
        break;
    case aa_backend::fsr3 :
        run_mv_reconstruct(cmd, params);
        run_fsr3(cmd, params);
        break;
    case aa_backend::fxaa :
        run_fxaa(cmd, params);
        break;
    }

    const rhi::image_barrier post_barriers[] = {
        app::make_image_barrier(output, rhi::image_layout::storage, rhi::image_layout::storage),
    };
    m_rhi.cmd_barriers(cmd, {.images = std::span(post_barriers)});
}

void app::pass_aa::on_render_display_size_changed(const uvec2 new_render_size, const uvec2 new_display_size)
{
    ZoneScoped;
    if (m_render_res == new_render_size && m_display_res == new_display_size)
    {
        return;
    }

    destroy_resources();
    create_resources(new_render_size, new_display_size);
}

void app::pass_aa::run_blit(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Copy AA");

    const rhi::image_barrier pre_barriers[] = {
        app::make_image_barrier(output, rhi::image_layout::storage, rhi::image_layout::common),
    };
    m_rhi.cmd_barriers(cmd, {.images = std::span(pre_barriers)});

    m_rhi.cmd_image_blit(cmd,
                         params.color,
                         output,
                         rhi::blit_image_info {
                             .src_extent = ivec3(m_render_res.x, m_render_res.y, 1),
                             .dst_extent = ivec3(m_display_res.x, m_display_res.y, 1),
                             .aspects    = rhi::image_aspect::color,
                             .filter     = rhi::sampler_filter::linear,
                         });

    const rhi::image_barrier post_barriers[] = {
        app::make_image_barrier(output, rhi::image_layout::common, rhi::image_layout::storage),
    };
    m_rhi.cmd_barriers(cmd, {.images = std::span(post_barriers)});
}

void app::pass_aa::run_fxaa(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "FXAA");

    const rhi::binding fxaa_pass_bindings[] = {
        rhi::binding(params.color, this->fxaa_color_sampler),
        rhi::binding(params.depth, this->fxaa_depth_sampler),
        rhi::binding(this->output),
    };

    const fxaa_params& fxaa_params = params.params.fxaa;
    const rhi::pipeline& fxaa_pass = m_pipelines[pso_id::fxaa_pipeline];

    m_rhi.cmd_bind_pso(cmd, fxaa_pass);
    m_rhi.cmd_push_bindings(cmd, fxaa_pass, std::span {fxaa_pass_bindings});

    m_rhi.cmd_push_constants(cmd, fxaa_pass, &fxaa_params.camera_near, sizeof(fxaa_params.camera_near), 0);
    m_rhi.cmd_push_constants(cmd, fxaa_pass, &m_render_res, sizeof(m_render_res), sizeof(fxaa_params.camera_near));

    m_rhi.cmd_dispatch(cmd, fxaa_pass, uvec3(m_render_res, 1));
}

void app::pass_aa::run_fsr3(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "FSR 3.1.4");

    auto fsr_params = params.params.ffx;
    const rhi::ffx_upscale_resources ffx_resources {
        .color          = params.color,
        .depth          = params.depth,
        .motion_vectors = motion_vectors,
        .output         = output,
    };

    fsr_params.ffx_upscale.jitter       = get_frame_jitter();
    fsr_params.ffx_upscale.render_size  = m_render_res;
    fsr_params.ffx_upscale.display_size = m_display_res;
    m_rhi.ffx_upscale_dispatch(ffx_context, cmd, fsr_params.ffx_upscale, ffx_resources);
}

void app::pass_aa::run_mv_reconstruct(const rhi::command_buffer cmd, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Motion vectors reconstruction");

    const rhi::image_barrier clear_barriers[] = {
        app::make_image_barrier(motion_vectors, rhi::image_layout::discard, rhi::image_layout::render_target_color),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span(clear_barriers)});
    const rhi::attachment_state_info clear_attachments[] = {
        {.attachment  = motion_vectors,
         .load_op     = rhi::resource_load_op::clear,
         .clear_value = {.color = {.f4 = vec4(0.0F)}}},
    };

    m_rhi.cmd_set_draw_state(
        cmd, clear_attachments, rhi::null_attachment_state_info, {0, 0, m_render_res.x, m_render_res.y});
    m_rhi.cmd_clear_draw_state(cmd);

    const rhi::image_barrier write_barriers[] = {
        app::make_image_barrier(motion_vectors, rhi::image_layout::render_target_color, rhi::image_layout::storage),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span(write_barriers)});

    const auto& mv_pass = m_pipelines[pso_id::mv_from_depth_pipeline];
    m_rhi.cmd_bind_pso(cmd, mv_pass);

    const rhi::binding mv_bindings[] = {
        rhi::binding(params.depth, fxaa_depth_sampler),
        rhi::binding(params.params.ffx.mv_matrices),
        rhi::binding(motion_vectors),
    };

    m_rhi.cmd_push_bindings(cmd, mv_pass, std::span {mv_bindings});
    m_rhi.cmd_push_constants(cmd, mv_pass, &m_render_res, sizeof(m_render_res), 0);
    m_rhi.cmd_dispatch(cmd, mv_pass, uvec3(m_render_res, 1));

    const rhi::image_barrier read_barriers[] = {
        app::make_image_barrier(motion_vectors, rhi::image_layout::storage, rhi::image_layout::common),
    };
    m_rhi.cmd_barriers(cmd, {.images = std::span(read_barriers)});
}

void app::pass_aa::destroy_resources()
{
    ZoneScoped;
    m_rhi.destroy_image(m_ctx, output);
    m_rhi.destroy_image(m_ctx, motion_vectors);
    m_rhi.ffx_destroy_context(ffx_context);
}

void app::pass_aa::create_resources(const uvec2 render_res, const uvec2 display_res)
{
    ZoneScoped;
    m_render_res  = render_res;
    m_display_res = display_res;

    motion_vectors = create_color_image(m_rhi,
                                        render_res,
                                        rhi::image_format::r16g16sf,
                                        m_ctx,
                                        "motion vectors",
                                        rhi::image_usage::attachment_color | rhi::image_usage::transfer_src);

    output      = app::create_color_image(m_rhi, display_res, rhi::image_format::b10g11r11uf, m_ctx, "AA output");
    ffx_context = *m_rhi.ffx_create_upscale_context(m_ctx,
                                                    rhi::ffx_upscale_desc {
                                                        .display_size    = display_res,
                                                        .max_render_size = render_res,
                                                    });
}
