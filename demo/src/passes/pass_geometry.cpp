#include <passes/pass_geometry.hpp>
#include <passes/scoped_label.hpp>
#include <tracy/Tracy.hpp>
#include <utils.hpp>

#include <shaders/bindings/draw.hlsl>
#include <shaders/constants.hlsl.h>

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
                                  const loader::scene_counters& counters, const uvec2 render_res)
    : m_ctx(context)
    , m_rhi(impl)
    , m_pipelines(pipelines)
{
    ZoneScoped;
    create_resources(render_res);

    mesh_draw_count = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = sizeof(u32[shader_constants::kMatClassCount]),
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

    mesh_dispatch_count = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = sizeof(u32[shader_constants::kMatClassCount * 3]),
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_dispatch_count",
        });

    m_spd_counter = m_rhi
                        .create_buffer(m_ctx,
                                       {
                                           .size        = sizeof(u32),
                                           .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
                                           .dbg_name    = "spd_counter",
                                       })
                        .value.id;

    mesh_visibility = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = (counters.instances + 31) / 8,
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_visibility",
        });

    meshlets_visibility = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = (counters.max_onscreen_meshlets + 31) / 8,
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "meshlets_visibility",
        });

    mesh_draw_commands = *m_rhi.create_buffer(
        m_ctx,
        {
            .size = (counters.buckets_offset.back() + counters.buckets.back())
                  * sizeof(shader_types::DrawTaskCommandIndirect),
            .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "mesh_draw_commands",
        });

    late_draw_count = *m_rhi.create_buffer(
        m_ctx,
        {
            .size        = sizeof(u32[shader_constants::kMatClassCount]),
            .usage_flags = rhi::buffer_usage::indirect | rhi::buffer_usage::copy_dst | rhi::buffer_usage::shader_rw,
            .dbg_name    = "late_draw_count",
        });

    // Fits: an instance either has early tasks to retest or late tasks of its own, both bounded by its bucket share
    late_draw_commands = *m_rhi.create_buffer(
        m_ctx,
        {
            .size = (counters.buckets_offset.back() + counters.buckets.back())
                  * sizeof(shader_types::DrawTaskCommandIndirect),
            .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "late_draw_commands",
        });

    u32 biggest_bucket = 0;
    for (auto& entry : counters.buckets)
    {
        biggest_bucket = core::max(biggest_bucket, entry);
    }

    indexed_draw_commands = *m_rhi.create_buffer(
        m_ctx,
        {
            .size = biggest_bucket * sizeof(shader_types::DrawIndexedIndirect) * shader_constants::kTaskWorkGroups / 2,
            .usage_flags = rhi::buffer_usage::copy_dst | rhi::buffer_usage::indirect | rhi::buffer_usage::shader_rw,
            .dbg_name    = "indexed_draw_commands",
        });
}

void app::pass_geometry::shutdown()
{
    ZoneScoped;
    destroy_resources();

    auto spd_ref = rhi::buffer {.id = m_spd_counter.value()};
    m_rhi.destroy_buffer(m_ctx, spd_ref);

    m_rhi.destroy_buffer(m_ctx, mesh_draw_count);
    m_rhi.destroy_buffer(m_ctx, mesh_visibility);
    m_rhi.destroy_buffer(m_ctx, mesh_draw_commands);
    m_rhi.destroy_buffer(m_ctx, mesh_dispatch_count);
    m_rhi.destroy_buffer(m_ctx, meshlets_visibility);
    m_rhi.destroy_buffer(m_ctx, indexed_count_buffer);
    m_rhi.destroy_buffer(m_ctx, indexed_draw_commands);
    m_rhi.destroy_buffer(m_ctx, late_draw_count);
    m_rhi.destroy_buffer(m_ctx, late_draw_commands);
}

void app::pass_geometry::draw(const rhi::command_buffer cmd, const common_resources& res,
                              const loader::scene_counters& counters, const draw_params& params)
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "V-buffer");

    const bool occlusion_cull_pass = params.flags & app::gpass_flags::hizb_cull;
    const bool use_mesh_shaders    = params.flags & app::gpass_flags::use_mesh_shaders;

    pass_state state {
        .late_pass      = occlusion_cull_pass,
        .prev_hizb_mode = static_cast<bool>(params.flags & app::gpass_flags::prev_hizb),
    };

    // Prev-frame HiZB mode: the late pass consumes the list the early task / mesh_to_indexed shaders started
    const bool use_late_list = state.late_pass && state.prev_hizb_mode;
    state.draw_commands      = use_late_list ? late_draw_commands : mesh_draw_commands;
    state.draw_count         = use_late_list ? late_draw_count : mesh_draw_count;

    if (params.flags & app::gpass_flags::any_cull)
    {
        if (params.flags & app::gpass_flags::cull)
        {
            on_pre_draw(cmd);
        }

        if (occlusion_cull_pass)
        {
            // Early pass task / mesh_to_indexed shaders append to the late list, late cull & patch passes continue it
            constexpr rhi::global_barrier early_to_late_barrier {
                .before = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::all_graphics,
                           .access = rhi::barrier_access::storage_write                                    },
                .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::all_graphics,
                           .access = rhi::barrier_access::storage_read | rhi::barrier_access::storage_write},
            };

            const rhi::barrier_batch early_to_late_barriers {
                .globals = {&early_to_late_barrier, 1}
            };
            m_rhi.cmd_barriers(cmd, early_to_late_barriers);
        }

        cull_scene(cmd, res, counters, state);
        patch_task_count(cmd, state);
    }

    for (const auto m_class : {shader_constants::kMatClassOpaque, shader_constants::kMatClassMasked})
    {
        if (!use_mesh_shaders)
        {
            build_indexed_draw_commands(cmd, res, counters, m_class, state);
        }

        const auto& render_pipeline =
            gpass_get_render_pipeline(m_pipelines, m_class, occlusion_cull_pass, use_mesh_shaders);
        const auto push_constants = shader_types::DrawPushConstants {
            params.projection * params.view, counters.buckets_offset[m_class], m_class};

        m_rhi.cmd_bind_pso(cmd, render_pipeline);
        m_rhi.cmd_push_constants(cmd, render_pipeline, &push_constants, sizeof(push_constants), 0);
        m_rhi.cmd_set_cull_mode(
            cmd, m_class == shader_constants::kMatClassOpaque ? rhi::cull_mode::back : rhi::cull_mode::none);

        draw_scene(cmd, res, render_pipeline, counters, m_class, use_mesh_shaders, state);
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
    for (auto& pyramid : m_hizb)
    {
        destroy_depth_pyramid(m_rhi, pyramid, m_ctx);
    }
}

void app::pass_geometry::create_resources(const uvec2 render_res)
{
    ZoneScoped;
    this->m_render_res = render_res;
    this->v_buffer     = create_vis_buffer_image(m_rhi, render_res, m_ctx);
    this->depth        = create_depth_image(m_rhi, render_res, rhi::image_format::d32sf, m_ctx);

    for (u32 i = 0; i < COUNT_OF(m_hizb); ++i)
    {
        m_hizb[i]       = create_depth_pyramid(m_rhi, render_res, rhi::image_format::r32sf, m_ctx);
        m_hizb_valid[i] = false;
    }
}

void app::pass_geometry::on_pre_draw(const rhi::command_buffer cmd) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Prepare V-buffer resources");

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

    // Both passes bind both pyramids, make sure freshly (re)created ones aren't in an undefined layout
    for (u32 i = 0; i < COUNT_OF(m_hizb); ++i)
    {
        if (!m_hizb_valid[i])
        {
            const rhi::image_barrier hizb_barrier = app::make_image_barrier(m_hizb[i].image, rhi::image_layout::common);
            const rhi::barrier_batch hizb_barriers {
                .images = {&hizb_barrier, 1}
            };
            m_rhi.cmd_barriers(cmd, hizb_barriers);
        }
    }
}

void app::pass_geometry::build_hzb(const rhi::command_buffer cmd)
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Build HiZB");

    if (!m_spd_counter.get_flag())
    {
        m_spd_counter.set_flag(true);
        app::zero_buffer(m_rhi, cmd, rhi::buffer {.id = m_spd_counter.value()});
    }

    const depth_pyramid_data& hizb = m_hizb[m_hizb_cur];

    const rhi::image_barrier img_barrier {
        app::make_image_barrier(hizb.image, rhi::image_layout::common),
    };

    const rhi::barrier_batch barriers {
        .images = {&img_barrier, 1},
    };

    m_rhi.cmd_barriers(cmd, barriers);

    constexpr u32 kSpdMaxMips = 12;
    constexpr u32 kSpdTile    = 64;
    constexpr u32 kSpdThreads = 256;

    const u32 mips = core::min<u32>(hizb.pyramid_count, kSpdMaxMips);
    assert2(mips == hizb.pyramid_count);

    rhi::binding bindings[2 + kSpdMaxMips];
    bindings[0] = rhi::binding(rhi::attachment(this->depth), hizb.sampler);
    bindings[1] = rhi::binding(rhi::buffer {.id = m_spd_counter.value()});
    for (u32 i = 0; i < kSpdMaxMips; ++i)
    {
        bindings[2 + i] = rhi::binding(rhi::attachment(hizb.views[core::min(i, mips - 1)]));
    }

    const auto& spd_pipeline = m_pipelines[pso_id::build_hizb_pipeline];
    const uvec2 groups       = (uvec2(hizb.base_size) * 2U + (kSpdTile - 1U)) / kSpdTile;

    const struct
    {
        uvec2 src_size;
        uvec2 hiz_size;
        u32 mips;
        u32 workgroups;
    } pc {.src_size = m_render_res, .hiz_size = uvec2(hizb.base_size), .mips = mips, .workgroups = groups.x * groups.y};

    m_rhi.cmd_bind_pso(cmd, spd_pipeline);
    m_rhi.cmd_push_bindings(cmd, spd_pipeline, std::span {bindings});
    m_rhi.cmd_push_constants(cmd, spd_pipeline, &pc, sizeof(pc), 0);
    m_rhi.cmd_dispatch(cmd, spd_pipeline, {groups.x * kSpdThreads, groups.y, 1});

    constexpr rhi::global_barrier done_barrier {
        .before = {.stages = rhi::barrier_stage::compute_shader, .access = rhi::barrier_access::storage_write},
        .after =
            {
                   .stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::all_graphics,
                   .access = rhi::barrier_access::sampled_read | rhi::barrier_access::storage_read,
                   },
    };

    const rhi::barrier_batch done_barriers {
        .globals = {&done_barrier, 1}
    };

    m_rhi.cmd_barriers(cmd, done_barriers);

    m_hizb_valid[m_hizb_cur] = true;
}

void app::pass_geometry::patch_task_count(const rhi::command_buffer cmd, const pass_state& state) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Patch task dispatch count");

    const rhi::pipeline& patch_pass = m_pipelines[pso_id::task_count_patch_pipeline];

    const rhi::binding bindings[] = {
        state.draw_count,
        mesh_dispatch_count,
    };

    m_rhi.cmd_bind_pso(cmd, patch_pass);
    m_rhi.cmd_push_bindings(cmd, patch_pass, std::span {bindings});
    m_rhi.cmd_dispatch(cmd, patch_pass, uvec3(1));

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

void app::pass_geometry::cull_scene(const rhi::command_buffer cmd, const common_resources& res,
                                    const loader::scene_counters& counters, const pass_state& state) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Cull scene");

    // Prev-frame HiZB mode: the late pass continues the list early task / mesh_to_indexed shaders started
    if (!(state.late_pass && state.prev_hizb_mode))
    {
        u32 update_data[shader_constants::kMatClassCount];
        core::cx_fill(&update_data[0], update_data + COUNT_OF(update_data), 0);

        constexpr auto update_range = u64vec2(0, sizeof(update_data));

        constexpr auto kCounterUsers =
            rhi::barrier_stage::copy | rhi::barrier_stage::compute_shader | rhi::barrier_stage::all_graphics;
        constexpr auto kCounterAccess =
            rhi::barrier_access::copy_write | rhi::barrier_access::storage_read | rhi::barrier_access::storage_write;

        rhi::global_barrier pre_barrier {
            .before = {.stages = kCounterUsers,            .access = kCounterAccess                 },
            .after  = {.stages = rhi::barrier_stage::copy, .access = rhi::barrier_access::copy_write}
        };
        const rhi::barrier_batch pre_barriers {
            .globals = {&pre_barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, pre_barriers);
        m_rhi.cmd_update_buffer(cmd, mesh_draw_count, update_range, update_data);
        if (state.prev_hizb_mode)
        {
            m_rhi.cmd_update_buffer(cmd, late_draw_count, update_range, update_data);
        }

        // all_graphics: early task shaders append to the late list
        rhi::global_barrier update_barrier {
            .before = {.stages = rhi::barrier_stage::copy,                                              .access = rhi::barrier_access::copy_write},
            .after  = {.stages = rhi::barrier_stage::compute_shader | rhi::barrier_stage::all_graphics,
                       .access = rhi::barrier_access::storage_read | rhi::barrier_access::storage_write                                          }
        };
        const rhi::barrier_batch update_barriers {
            .globals = {&update_barrier, 1}
        };

        m_rhi.cmd_barriers(cmd, update_barriers);
    }

    // Early pass tests against the previous frame HiZB (prev-frame HiZB mode only), late pass against this frame's
    const depth_pyramid_data& test_hizb = state.late_pass ? cur_hizb() : prev_hizb();

    descriptor_bindings bindings;
    bindings.bind_next(res.primitives);
    bindings.bind_next(res.instances);
    bindings.bind_next(res.materials);
    bindings.bind_next(state.draw_count);
    bindings.bind_next(mesh_visibility);
    bindings.bind_next(res.frame_cull_data_buffer().buffer);
    bindings.bind_next(state.draw_commands);
    bindings.bind_next(rhi::binding(test_hizb.image, test_hizb.sampler));

    const rhi::pipeline& cull_pass =
        state.late_pass ? m_pipelines[pso_id::task_occlusion_cull_pipeline] : m_pipelines[pso_id::task_cull_pipeline];

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
                                    const u32 material_class, const bool mesh_pipeline, const pass_state& state) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Draw scene");

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
        std::span {color_attachments, COUNT_OF(color_attachments)},
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

    const depth_pyramid_data& cur_pyramid  = cur_hizb();
    const depth_pyramid_data& prev_pyramid = prev_hizb();

    bindings.bind_at(state.draw_commands, shader_bindings::draw::kDrawBinding, mesh_pipeline);
    bindings.bind_at(state.draw_count, shader_bindings::draw::kCountBinding, mesh_pipeline);
    bindings.bind_at(meshlets_visibility, shader_bindings::draw::kVisibilityBinding, mesh_pipeline);
    bindings.bind_at(
        rhi::binding(cur_pyramid.image, cur_pyramid.sampler), shader_bindings::draw::kHiZBinding, mesh_pipeline);
    bindings.bind_at(res.frame_cull_data_buffer().buffer, shader_bindings::draw::kCullBinding, mesh_pipeline);
    bindings.bind_at(
        rhi::binding(prev_pyramid.image, prev_pyramid.sampler), shader_bindings::draw::kPrevHiZBinding, mesh_pipeline);
    bindings.bind_at(late_draw_commands, shader_bindings::draw::kLateDrawBinding, mesh_pipeline);
    bindings.bind_at(late_draw_count, shader_bindings::draw::kLateCountBinding, mesh_pipeline);

    bindings.bind_at(indexed_draw_commands, shader_bindings::draw::kDrawBinding, !mesh_pipeline);

    if (mesh_pipeline)
    {
        m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
        m_rhi.cmd_draw_mesh_indirect(cmd, mesh_dispatch_count, material_class * 3 * sizeof(u32), 1, 0);
    }
    else
    {
        m_rhi.cmd_push_bindings(cmd, pipeline, bindings.get());
        m_rhi.cmd_bind_index(cmd, res.index);
#if defined(__APPLE__)
        m_rhi.cmd_draw_indexed_indirect(cmd,
                                        indexed_draw_commands,
                                        0,
                                        counters.buckets[material_class] * shader_constants::kTaskWorkGroups / 2,
                                        sizeof(shader_types::DrawIndexedIndirect));
#else
        m_rhi.cmd_draw_indexed_indirect_count(cmd,
                                              indexed_draw_commands,
                                              0,
                                              indexed_count_buffer,
                                              sizeof(u32),
                                              counters.buckets[material_class] * shader_constants::kTaskWorkGroups / 2,
                                              sizeof(shader_types::DrawIndexedIndirect));
#endif
    }

    m_rhi.cmd_clear_draw_state(cmd);
}

void app::pass_geometry::build_indexed_draw_commands(const rhi::command_buffer cmd, const common_resources& res,
                                                     const loader::scene_counters& counters, const u32 material_class,
                                                     const pass_state& state) const
{
    ZoneScoped;
    SCOPED_GPU_PASS(m_rhi, cmd, m_ctx, "Build indexed draw commands");

#if defined(__APPLE__)
    app::zero_buffer(m_rhi, cmd, indexed_draw_commands);
#endif
    app::zero_buffer(m_rhi, cmd, indexed_count_buffer, sizeof(u32));

    descriptor_bindings bindings;
    bindings.bind_next(res.meshlets);
    bindings.bind_next(res.meshlets_payload);
    bindings.bind_next(res.primitives);
    bindings.bind_next(res.instances);
    bindings.bind_next(state.draw_commands);
    bindings.bind_next(state.draw_count);
    bindings.bind_next(res.frame_cull_data_buffer().buffer);
    bindings.bind_next(res.index);
    bindings.bind_next(indexed_draw_commands);
    bindings.bind_next(indexed_count_buffer);
    bindings.bind_next(meshlets_visibility);
    bindings.bind_next(rhi::binding(cur_hizb().image, cur_hizb().sampler));
    bindings.bind_next(rhi::binding(prev_hizb().image, prev_hizb().sampler));
    bindings.bind_next(late_draw_commands);
    bindings.bind_next(late_draw_count);

    pso_id id;
    switch (material_class)
    {
    case shader_constants::kMatClassMasked :
    case shader_constants::kMatClassTranslucent :
        id = (state.late_pass ? pso_id::mesh_to_indexed_ds_late_pipeline : pso_id::mesh_to_indexed_ds_pipeline);
        break;
    default :
    case shader_constants::kMatClassOpaque :
        id = (state.late_pass ? pso_id::mesh_to_indexed_late_pipeline : pso_id::mesh_to_indexed_pipeline);
        break;
    }

    const rhi::pipeline& fill_pass = m_pipelines[id];

    const struct push_constants
    {
        u32 cmd_offset;
        u32 count_offset;
    } pc(counters.buckets_offset[material_class], material_class);

    m_rhi.cmd_bind_pso(cmd, fill_pass);
    m_rhi.cmd_push_constants(cmd, fill_pass, &pc, sizeof(pc), 0);

    m_rhi.cmd_push_bindings(cmd, fill_pass, bindings.get());
    m_rhi.cmd_dispatch_indirect(cmd, mesh_dispatch_count, material_class * 3 * sizeof(u32));

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
