#include <app/render.hpp>
#include <passes/pass_light_resolve.hpp>

app::pass_light_resolve::pass_light_resolve(const rhi::impl& impl, const rhi::context context, const uvec2 render_res)
    : m_ctx(context)
    , m_rhi(impl)
{
    ZoneScoped;
    create_resources(render_res);
    depth_sampler = *m_rhi.create_sampler(m_ctx,
                                          {
                                              .filter       = rhi::sampler_filter::nearest,
                                              .mipmap_mode  = rhi::sampler_mipmap_mode::nearest,
                                              .address_mode = rhi::sampler_address_mode::clamp_to_border,
                                          });
}

void app::pass_light_resolve::shutdown()
{
    ZoneScoped;
    destroy_resources();
    m_rhi.destroy_sampler(m_ctx, depth_sampler);
}

void app::pass_light_resolve::on_render_size_changed(const uvec2 new_render_size)
{
    ZoneScoped;
    if (m_render_res == new_render_size)
    {
        return;
    }

    destroy_resources();
    create_resources(new_render_size);
}

void app::pass_light_resolve::draw(const rhi::command_buffer cmd, const common_resources& res,
                                   const draw_params& params) const
{
    ZoneScoped;
    constexpr rhi::global_barrier pre_barrier {
        .before = {.stages = rhi::barrier_stage::color_attachment | rhi::barrier_stage::late_depth_stencil,
                   .access = rhi::barrier_access::color_attachment_write | rhi::barrier_access::depth_stencil_write},
        .after  = {.stages = rhi::barrier_stage::compute_shader,
                   .access = rhi::barrier_access::storage_read | rhi::barrier_access::sampled_read                 },
    };

    const rhi::image_barrier transition_barriers[] {
        app::make_image_barrier(this->output, rhi::image_layout::common, rhi::image_aspect::color),
    };

    m_rhi.cmd_barriers(cmd,
                       rhi::barrier_batch {
                           .images  = std::span {transition_barriers},
                           .globals = {&pre_barrier, 1},
    });

    const rhi::binding resolve_pass_bindings[] = {
        rhi::binding(rhi::attachment(output)),
        rhi::binding(rhi::attachment(params.color)),
        params.indices,
        res.vertex,
        res.meshlets,
        res.meshlets_payload,
        res.primitives,
        res.instances,
        res.materials,
        res.world_data_buffer().buffer,
        rhi::binding(res.bindless_sampler),
        rhi::binding(params.depth, depth_sampler),
        params.brdf_lookup,
        params.env_cube,
        params.env_conv,
        params.env_prefilter,
    };

    const rhi::pipeline& resolve_pass = params.pipeline;

    m_rhi.cmd_bind_pso(cmd, resolve_pass);
    m_rhi.cmd_push_bindings(cmd, resolve_pass, std::span {resolve_pass_bindings});
    m_rhi.cmd_push_bindless_set(cmd, resolve_pass, res.bindless_textures, 1);

    const auto push_constants = shader_types::ResolvePassPushConstants(params.view, params.projection);
    m_rhi.cmd_push_constants(cmd, resolve_pass, &push_constants, sizeof(push_constants), 0);

    m_rhi.cmd_dispatch(cmd, resolve_pass, uvec3(m_render_res, 1));

    constexpr rhi::global_barrier post_barrier {
        .before = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::storage_write},
        .after  = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::sampled_read },
    };

    m_rhi.cmd_barriers(cmd,
                       rhi::barrier_batch {
                           .globals = {&post_barrier, 1}
    });
}

void app::pass_light_resolve::destroy_resources()
{
    ZoneScoped;
    m_rhi.destroy_image(m_ctx, output);
}

void app::pass_light_resolve::create_resources(const uvec2 render_res)
{
    ZoneScoped;
    m_render_res = render_res;
    output       = app::create_color_image(m_rhi, render_res, rhi::image_format::b10g11r11uf, m_ctx);
}
