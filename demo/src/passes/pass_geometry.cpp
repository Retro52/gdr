#include <passes/pass_geometry.hpp>
#include <passes/scoped_label.hpp>
#include <shaders/constants.hlsl.h>
#include <tracy/Tracy.hpp>

#include <shaders/bindings/draw.hlsl>

static const rhi::pipeline& gpass_get_render_pipeline(const app::pso_data& pipelines, const u32 material_class,
                                                      const bool occlusion_cull, const bool enable_meshlets)
{
    ZoneScoped;
    app::pso_id id {};
    switch (material_class)
    {
    case shader_constants::kMatClassMasked :
    case shader_constants::kMatClassTranslucent :

    {
        id = enable_meshlets
               ? (occlusion_cull ? app::pso_id::task_render_ds_late_pipeline : app::pso_id::task_render_ds_pipeline)
               : app::pso_id::indexed_render_ds_pipeline;
        break;
    }
    default :
    case shader_constants::kMatClassOpaque :
    {
        id = enable_meshlets
               ? (occlusion_cull ? app::pso_id::task_render_late_pipeline : app::pso_id::task_render_pipeline)
               : app::pso_id::indexed_render_pipeline;
        break;
    }
    }

    return pipelines[id];
}

app::pass_geometry::pass_geometry(const rhi::impl& impl, const app::pso_data& pipelines, const rhi::context context,
                                  const uvec2 render_res)
    : m_ctx(context)
    , m_rhi(impl)
    , m_pipelines(pipelines)
{
    ZoneScoped;
    create_resources(render_res);

    mesh_draw_count = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = sizeof(u32[shader_constants::kMatClassCount * 3]),
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_draw_count",
        });

    indexed_count_buffer = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = sizeof(u32[2]),
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "indexed_count_buffer",
        });

    mesh_visibility = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = kMaxInstances / 8,
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_visibility",
        });

    meshlets_visibility = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = kMaxMeshlets / 8,
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "meshlets_visibility",
        });

    indices = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = 96_MB,
            .usage_flags = rhi::buffer_usage::index | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "indices",
        });

    mesh_draw_commands = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = kMaxMeshlets * sizeof(shader_types::DrawTaskCommandIndirect),
            .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_draw_commands",
        });

    indexed_draw_commands = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = kMaxMeshlets * sizeof(shader_types::DrawIndexedIndirect),
            .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "indexed_draw_commands",
        });
}

void app::pass_geometry::shutdown()
{
    ZoneScoped;
    destroy_resources();

    m_rhi.destroy_buffer(m_ctx, indices);
    m_rhi.destroy_buffer(m_ctx, mesh_draw_count);
    m_rhi.destroy_buffer(m_ctx, mesh_visibility);
    m_rhi.destroy_buffer(m_ctx, mesh_draw_commands);
    m_rhi.destroy_buffer(m_ctx, meshlets_visibility);
    m_rhi.destroy_buffer(m_ctx, indexed_count_buffer);
    m_rhi.destroy_buffer(m_ctx, indexed_draw_commands);
}

void app::pass_geometry::draw(const rhi::command_buffer cmd, const common_resources& res,
                              const loader::scene_counters& counters, const draw_params& params) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "V-buffer pass");

    const bool occlusion_cull_pass = params.flags & app::gpass_flags::hizb_cull;
    const bool use_mesh_shaders    = params.flags & app::gpass_flags::use_mesh_shaders;

    if (params.flags & app::gpass_flags::any_cull)
    {
        if (params.flags & app::gpass_flags::cull)
        {
            on_pre_draw(cmd);
        }

        cull_scene(cmd, res, counters, occlusion_cull_pass);
    }

    for (const auto m_class : {shader_constants::kMatClassOpaque, shader_constants::kMatClassMasked})
    {
        if (!use_mesh_shaders)
        {
            build_indexed_draw_commands(cmd, res, counters, m_class, occlusion_cull_pass);
        }

        const auto& render_pipeline =
            gpass_get_render_pipeline(m_pipelines, m_class, occlusion_cull_pass, use_mesh_shaders);
        const auto push_constants = shader_types::DrawPushConstants {
            params.projection * params.view,
            counters.buckets_offset[m_class],
        };

        m_rhi.cmd_bind_pso(cmd, render_pipeline);
        m_rhi.cmd_push_constants(cmd, render_pipeline, &push_constants, sizeof(push_constants), 0);
        m_rhi.cmd_set_cull_mode(
            cmd, m_class == shader_constants::kMatClassOpaque ? rhi::cull_mode::back : rhi::cull_mode::none);

        draw_scene(cmd, res, render_pipeline, counters, m_class, use_mesh_shaders);
    }

    if (params.flags & app::gpass_flags::build_hizb)
    {
        build_hzb(cmd);
    }
}

void app::pass_geometry::on_render_size_changed(const uvec2 new_render_size)
{
    ZoneScoped;
    if (m_render_res == new_render_size)
    {
        return;
    }

    destroy_resources();
    create_resources(new_render_size);
}

void app::pass_geometry::destroy_resources()
{
    ZoneScoped;
    m_rhi.destroy_image(m_ctx, this->depth);
    m_rhi.destroy_image(m_ctx, this->v_buffer);
    destroy_depth_pyramid(m_rhi, this->hizb, m_ctx);
}

void app::pass_geometry::create_resources(const uvec2 render_res)
{
    ZoneScoped;
    this->m_render_res = render_res;
    this->v_buffer     = create_vis_buffer_image(m_rhi, render_res, m_ctx);
    this->depth        = create_depth_image(m_rhi, render_res, rhi::image_format::d32sf, m_ctx);
    this->hizb         = create_depth_pyramid(m_rhi, render_res, rhi::image_format::r32sf, m_ctx);
}

void app::pass_geometry::on_pre_draw(const rhi::command_buffer cmd) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "on_pre_draw");

    constexpr rhi::ds_clear_value kDSClear    = {.depth = 0.0F, .stencil = 0};
    constexpr rhi::color_clear_value kVBClear = {.u4 = uvec4(~0U)};

    const rhi::image_barrier transition_barriers[] {
        app::make_image_barrier(this->v_buffer, rhi::image_layout::common, rhi::image_aspect::color),
        app::make_image_barrier(this->depth, rhi::image_layout::common, rhi::image_aspect::depth),
    };

    m_rhi.cmd_barriers(cmd, {.images = std::span {transition_barriers}});

    m_rhi.cmd_clear_depth_attachment(cmd, this->depth, kDSClear);
    m_rhi.cmd_clear_color_attachment(cmd, this->v_buffer, kVBClear);

    const rhi::image_barrier clear_barriers[] {
        app::make_image_barrier(this->depth,
                                rhi::image_layout::current,
                                rhi::image_layout::current,
                                rhi::barrier_stage::copy,
                                rhi::barrier_stage::early_depth_stencil,
                                rhi::barrier_access::copy_write,
                                rhi::barrier_access::depth_stencil_read | rhi::barrier_access::depth_stencil_write,
                                rhi::image_aspect::depth),
        app::make_image_barrier(this->v_buffer,
                                rhi::image_layout::current,
                                rhi::image_layout::current,
                                rhi::barrier_stage::copy,
                                rhi::barrier_stage::color_attachment,
                                rhi::barrier_access::copy_write,
                                rhi::barrier_access::color_attachment_read
                                    | rhi::barrier_access::color_attachment_write,
                                rhi::image_aspect::color),

    };

    m_rhi.cmd_barriers(cmd, {.images = std::span {clear_barriers}});
    app::zero_buffer(m_rhi, cmd, indexed_count_buffer);
}

void app::pass_geometry::build_hzb(rhi::command_buffer cmd) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "build_hzb");

    constexpr rhi::global_barrier barrier {
        .before =
            {
                     .stages = rhi::barrier_stage::early_depth_stencil | rhi::barrier_stage::late_depth_stencil,
                     .access = rhi::barrier_access::depth_stencil_write,
                     },
        .after = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::sampled_read},
    };

    const rhi::image_barrier img_barrier {
        app::make_image_barrier(this->hizb.image, rhi::image_layout::common),
    };

    const rhi::barrier_batch barriers {
        .images  = {&img_barrier, 1},
        .globals = {&barrier,     1},
    };

    m_rhi.cmd_barriers(cmd, barriers);

    const auto& depth_reduce_pipeline = m_pipelines[pso_id::depth_reduce_pipeline];
    m_rhi.cmd_bind_pso(cmd, depth_reduce_pipeline);

    for (i32 i = 0; i < this->hizb.pyramid_count; ++i)
    {
        const rhi::binding cull_pass_bindings[] = {
            rhi::binding(i == 0 ? rhi::attachment(this->depth) : rhi::attachment(this->hizb.views[i - 1]),
                         this->hizb.sampler),
            rhi::binding(this->hizb.views[i], this->hizb.sampler),
        };

        m_rhi.cmd_push_bindings(cmd, depth_reduce_pipeline, std::span {cull_pass_bindings});

        const ivec2 out_size = glm::max(this->hizb.base_size >> i, ivec2(1));
        const vec2 push_constants(out_size);
        m_rhi.cmd_push_constants(cmd, depth_reduce_pipeline, &push_constants, sizeof(push_constants), 0);
        m_rhi.cmd_dispatch(cmd, depth_reduce_pipeline, {out_size.x, out_size.y, 1});

        constexpr rhi::global_barrier dp_barrier {
            .before = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::storage_write},
            .after  = {.stages = rhi::barrier_stage::compute_shader,
                       .access = rhi::barrier_access::sampled_read | rhi::barrier_access::storage_read           },
        };

        const rhi::barrier_batch dp_barriers {
            .globals = {&dp_barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, dp_barriers);
    }
}

void app::pass_geometry::cull_scene(const rhi::command_buffer cmd, const common_resources& res,
                                    const loader::scene_counters& counters, const bool occlusion_cull) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "cull_scene");

    reset_draw_count_buffer(m_rhi, cmd, mesh_draw_count);

    descriptor_bindings bindings;
    bindings.bind_next(res.primitives);
    bindings.bind_next(res.instances);
    bindings.bind_next(res.materials);
    bindings.bind_next(mesh_draw_count);
    bindings.bind_next(mesh_visibility);
    bindings.bind_next(res.frame_cull_data_buffer().buffer);
    bindings.bind_next(mesh_draw_commands);
    bindings.bind_next(rhi::binding(this->hizb.image, this->hizb.sampler), occlusion_cull);

    const rhi::pipeline& cull_pass =
        occlusion_cull ? m_pipelines[pso_id::task_occlusion_cull_pipeline] : m_pipelines[pso_id::task_cull_pipeline];

    m_rhi.cmd_bind_pso(cmd, cull_pass);
    m_rhi.cmd_push_bindings(cmd, cull_pass, bindings.get());
    m_rhi.cmd_push_constants(cmd, cull_pass, &counters.buckets_offset, sizeof(counters.buckets_offset), 0);

    m_rhi.cmd_dispatch(cmd, cull_pass, {static_cast<u32>(counters.instances), 1, 1});

    rhi::global_barrier barrier {
        .before = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::storage_write},
        .after  = {
                   .stages =
                rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect | rhi::barrier_stage::all_graphics,
                   .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read,
                   }
    };

    const rhi::barrier_batch barriers {
        .globals = {&barrier, 1}
    };
    m_rhi.cmd_barriers(cmd, barriers);
}

void app::pass_geometry::draw_scene(const rhi::command_buffer cmd, const common_resources& res,
                                    const rhi::pipeline& pipeline, const loader::scene_counters& counters,
                                    const u32 material_class, const bool mesh_pipeline) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "draw_scene");

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
        {.attachment = this->v_buffer,
         .load_op    = rhi::resource_load_op::load,
         .store_op   = rhi::resource_store_op::store},
    };
    m_rhi.cmd_set_draw_state(
        cmd,
        color_attachments,
        {.attachment = this->depth, .load_op = rhi::resource_load_op::load, .store_op = rhi::resource_store_op::store},
        {0, 0, m_render_res.x, m_render_res.y});

    if (material_class != shader_constants::kMatClassOpaque)
    {
        m_rhi.cmd_push_bindless_set(cmd, pipeline, res.bindless_textures, 1);
    }

    descriptor_bindings bindings;
    bindings.bind_at(res.vertex, shader_bindings::draw::kVertexBinding);
    bindings.bind_at(res.materials, shader_bindings::draw::kMaterialBinding);
    bindings.bind_at(res.meshlets, shader_bindings::draw::kMeshletBinding);
    bindings.bind_at(res.meshlets_payload, shader_bindings::draw::kMeshletDataBinding);
    bindings.bind_at(res.primitives, shader_bindings::draw::kPrimitiveBinding);
    bindings.bind_at(res.instances, shader_bindings::draw::kInstanceBinding);
    bindings.bind_at(rhi::binding(res.bindless_sampler), shader_bindings::draw::kTextureBinding);

    bindings.bind_at(mesh_draw_commands, shader_bindings::draw::kDrawBinding, mesh_pipeline);
    bindings.bind_at(meshlets_visibility, shader_bindings::draw::kVisibilityBinding, mesh_pipeline);
    bindings.bind_at(rhi::binding(hizb.image, hizb.sampler), shader_bindings::draw::kHiZBinding, mesh_pipeline);
    bindings.bind_at(res.frame_cull_data_buffer().buffer, shader_bindings::draw::kCullBinding, mesh_pipeline);

    bindings.bind_at(indexed_draw_commands, shader_bindings::draw::kDrawBinding, !mesh_pipeline);

    if (mesh_pipeline)
    {
        m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
        m_rhi.cmd_draw_mesh_indirect(cmd, mesh_draw_count, material_class * 3 * sizeof(u32), 1, 0);
    }
    else
    {
        m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
        m_rhi.cmd_bind_index(cmd, indices);

        // pipeline_statistics_query.begin_next(cmd);
#if defined(__APPLE__)
        m_rhi.cmd_draw_indexed_indirect(
            cmd, indexed_draw_commands, 0, counters.buckets[material_class], sizeof(shader_types::DrawIndexedIndirect));
#else
        m_rhi.cmd_draw_indexed_indirect_count(cmd,
                                              indexed_draw_commands,
                                              0,
                                              indexed_count_buffer,
                                              sizeof(u32),
                                              counters.buckets[material_class],
                                              sizeof(shader_types::DrawIndexedIndirect));
#endif
        // pipeline_statistics_query.end_and_advance(cmd);
    }

    m_rhi.cmd_clear_draw_state(cmd);
}

void app::pass_geometry::build_indexed_draw_commands(const rhi::command_buffer cmd, const common_resources& res,
                                                     const loader::scene_counters& counters, const u32 material_class,
                                                     const bool enable_occlusion_cull) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "build_indexed_draw_commands");

#if defined(__APPLE__)
    app::zero_buffer(m_rhi, cmd, indexed_draw_commands);
#endif
    app::zero_buffer(m_rhi, cmd, indexed_count_buffer, sizeof(u32));

    descriptor_bindings bindings;
    bindings.bind_next(res.meshlets);
    bindings.bind_next(res.meshlets_payload);
    bindings.bind_next(res.primitives);
    bindings.bind_next(res.instances);
    bindings.bind_next(mesh_draw_commands);
    bindings.bind_next(res.frame_cull_data_buffer().buffer);
    bindings.bind_next(indices);
    bindings.bind_next(indexed_draw_commands);
    bindings.bind_next(indexed_count_buffer);
    bindings.bind_next(meshlets_visibility);
    bindings.bind_next(rhi::binding(this->hizb.image, this->hizb.sampler), enable_occlusion_cull);

    pso_id id;
    switch (material_class)
    {
    case shader_constants::kMatClassMasked :
    case shader_constants::kMatClassTranslucent :
        id = (enable_occlusion_cull ? pso_id::mesh_to_indexed_ds_late_pipeline : pso_id::mesh_to_indexed_ds_pipeline);
        break;
    default :
    case shader_constants::kMatClassOpaque :
        id = (enable_occlusion_cull ? pso_id::mesh_to_indexed_late_pipeline : pso_id::mesh_to_indexed_pipeline);
        break;
    }

    const rhi::pipeline& fill_pass = m_pipelines[id];

    m_rhi.cmd_bind_pso(cmd, fill_pass);
    m_rhi.cmd_push_constants(
        cmd, fill_pass, &counters.buckets_offset[material_class], sizeof(counters.buckets_offset[material_class]), 0);

    m_rhi.cmd_push_bindings(cmd, fill_pass, bindings.get());
    m_rhi.cmd_dispatch_indirect(cmd, mesh_draw_count, material_class * 3 * sizeof(u32));

    constexpr rhi::global_barrier barrier {
        .before =
            {
                     .stages = rhi::barrier_stage::compute_shader,
                     .access = rhi::barrier_access::storage_write,
                     },
        .after =
            {
                     .stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::indirect
                        | rhi::barrier_stage::all_graphics,
                     .access = rhi::barrier_access::indirect_read | rhi::barrier_access::storage_read
                        | rhi::barrier_access::index_read,
                     },
    };

    const rhi::barrier_batch barriers {
        .globals = {&barrier, 1}
    };

    m_rhi.cmd_barriers(cmd, barriers);
}
