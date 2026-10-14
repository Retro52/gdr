#include <passes/common_resources.hpp>

app::common_resources::common_resources(const rhi::impl& rhi, const rhi::context ctx, const u32 frames_in_flight)
    : m_frames_in_flight(frames_in_flight)
{
    this->vertex           = *rhi.create_buffer(ctx, {.size = 128_MB, .dbg_name = "vertex"});
    this->meshlets         = *rhi.create_buffer(ctx, {.size = 16_MB, .dbg_name = "meshlets"});
    this->primitives       = *rhi.create_buffer(ctx, {.size = 1_MB, .dbg_name = "primitives"});
    this->instances        = *rhi.create_buffer(ctx, {.size = 48_MB, .dbg_name = "instances"});
    this->materials        = *rhi.create_buffer(ctx, {.size = 48_MB, .dbg_name = "materials"});
    this->meshlets_payload = *rhi.create_buffer(ctx, {.size = 128_MB, .dbg_name = "meshlets_payload"});

    this->bindless_textures = *rhi.create_bindless_set(ctx, 65536);
    this->bindless_sampler  = *rhi.create_sampler(ctx,
                                                 rhi::create_sampler_info {
                                                      .anisotropy_factor = 16,
                                                      .dbg_name          = "bindless_textures_sampler",
                                                 });

    for (u32 i = 0; i < frames_in_flight; i++)
    {
        world_data_buffers.emplace_back() =
            mapped_buffer::create(rhi, ctx, sizeof(shader_types::FrameWorldData), rhi::buffer_usage::shader_rw);

        frame_cull_data_buffers.emplace_back() =
            mapped_buffer::create(rhi, ctx, sizeof(shader_types::FrameCullData), rhi::buffer_usage::shader_rw);

        motion_vector_data_buffers.emplace_back() =
            mapped_buffer::create(rhi, ctx, sizeof(shader_types::MotionVectorsData), rhi::buffer_usage::shader_rw);
    }
}

void app::common_resources::next_frame()
{
    m_frame_id = (m_frame_id + 1) % m_frames_in_flight;
}
