#pragma once

#include <app/pso.hpp>
#include <app/render.hpp>
#include <passes/common_resources.hpp>
#include <render/rhi.hpp>
#include <scene/loader.hpp>

namespace app
{
    // clang-format off
    REGISTER_FLAGS(gpass_flags,
        none             = 0,
        cull             = 1 << 0,
        hizb_cull        = 1 << 1,
        build_hizb       = 1 << 2,
        use_mesh_shaders = 1 << 3,
        prev_hizb        = 1 << 4, // prev-frame HiZB visibility is active this frame (kPrevFrameHiZBBit)

        any_cull = hizb_cull | cull,

        post_hizb_indexed = hizb_cull,
        pre_hizb_indexed  = build_hizb | cull,

        post_hizb_mesh = use_mesh_shaders | hizb_cull,
        pre_hizb_mesh  = use_mesh_shaders | build_hizb | cull
    );
    // clang-format on

    struct pass_geometry
    {
        struct draw_params
        {
            glm::mat4 view;
            glm::mat4 projection;

            gpass_flags_bits flags = gpass_flags::none;
        };

        rhi::image depth    = rhi::null_image;
        rhi::image v_buffer = rhi::null_image;

        rhi::buffer mesh_draw_count      = rhi::null_buffer;  // Raw per-mat class counter of task shader dispatches
        rhi::buffer mesh_dispatch_count  = rhi::null_buffer;  // Patched thread count used to dispatch task shaders
        rhi::buffer indexed_count_buffer = rhi::null_buffer;  // Counter for DrawIndexedIndirectCount/ExecuteIndirect

        rhi::buffer mesh_visibility     = rhi::null_buffer;  // Bitfields: visible last frame. Prev HiZB: drawn early
        rhi::buffer meshlets_visibility = rhi::null_buffer;  // Bitfields mode only

        rhi::buffer mesh_draw_commands    = rhi::null_buffer;
        rhi::buffer indexed_draw_commands = rhi::null_buffer;

        // Prev-frame HiZB mode: late pass tasks. Early tasks with meshlets rejected by the previous frame HiZB,
        // followed by instances that weren't drawn early; same per-material class layout as mesh_draw_commands
        rhi::buffer late_draw_count    = rhi::null_buffer;
        rhi::buffer late_draw_commands = rhi::null_buffer;

        constexpr static u32 kMaxMeshlets  = ~static_cast<u32>(0) >> 10;
        constexpr static u32 kMaxInstances = kMaxMeshlets >> 2;

        pass_geometry(const rhi::impl& impl, const app::pso_data& pipelines, rhi::context context,
                      const loader::scene_counters& counters, uvec2 render_res);

        void shutdown();

        // Swaps the current & previous frame HiZB, call once per frame before drawing
        void next_frame() { m_hizb_cur ^= 1U; }

        void draw(rhi::command_buffer cmd, const common_resources& res, const loader::scene_counters& counters,
                  const draw_params& params);

        void on_render_size_changed(uvec2 new_render_size);

        // This frame's HiZB, built after the early pass (and after the late one in prev-frame HiZB mode)
        [[nodiscard]] const depth_pyramid_data& cur_hizb() const { return m_hizb[m_hizb_cur]; }

        // Last frame's HiZB, prev-frame HiZB mode tests against it
        [[nodiscard]] const depth_pyramid_data& prev_hizb() const { return m_hizb[m_hizb_cur ^ 1U]; }

        // Previous frame HiZB holds data, so prev-frame HiZB visibility can be used this frame
        [[nodiscard]] bool has_hizb_history() const { return m_hizb_valid[m_hizb_cur ^ 1U]; }

    private:
        // Which task list & counters a pass consumes
        struct pass_state
        {
            bool late_pass      = false;
            bool prev_hizb_mode = false;

            rhi::buffer draw_commands = rhi::null_buffer;
            rhi::buffer draw_count    = rhi::null_buffer;
        };

        void destroy_resources();

        void create_resources(uvec2 render_res);

        void on_pre_draw(rhi::command_buffer cmd) const;

        void build_hzb(rhi::command_buffer cmd);

        void patch_task_count(rhi::command_buffer cmd, const pass_state& state) const;

        void cull_scene(rhi::command_buffer cmd, const common_resources& res, const loader::scene_counters& counters,
                        const pass_state& state) const;

        void draw_scene(rhi::command_buffer cmd, const common_resources& res, const rhi::pipeline& pipeline,
                        const loader::scene_counters& counters, u32 material_class, bool mesh_pipeline,
                        const pass_state& state) const;

        void build_indexed_draw_commands(rhi::command_buffer cmd, const common_resources& res,
                                         const loader::scene_counters& counters, u32 material_class,
                                         const pass_state& state) const;

    private:
        uvec2 m_render_res             = uvec2(0);
        rhi::context m_ctx             = rhi::null_context;
        core::tagged_u64 m_spd_counter = 0;

        depth_pyramid_data m_hizb[2];
        u32 m_hizb_cur       = 0;
        bool m_hizb_valid[2] = {false, false};

        const rhi::impl& m_rhi;
        const app::pso_data& m_pipelines;
    };
}
