#include <passes/common_resources.hpp>

app::common_resources::common_resources(const rhi::impl& rhi, const rhi::context ctx,
                                        const loader::scene_counters& counters, const u32 frames_in_flight)
    : m_frames_in_flight(frames_in_flight)
{
    this->vertex =
        *rhi.create_buffer(ctx, {.size = counters.vertices * sizeof(shader_types::Vertex), .dbg_name = "vertex"});
    this->meshlets =
        *rhi.create_buffer(ctx, {.size = counters.meshlets * sizeof(shader_types::Meshlet), .dbg_name = "meshlets"});
    this->primitives = *rhi.create_buffer(
        ctx, {.size = counters.primitives * sizeof(shader_types::MeshData), .dbg_name = "primitives"});
    this->instances = *rhi.create_buffer(
        ctx, {.size = counters.instances * sizeof(shader_types::MeshInstance), .dbg_name = "instances"});
    this->materials = *rhi.create_buffer(
        ctx, {.size = counters.materials * sizeof(shader_types::MeshMaterial), .dbg_name = "materials"});

    this->meshlets_payload  = *rhi.create_buffer(ctx,
                                                 {.size = counters.meshlets * shader_constants::kMaxIndicesPerMeshlet
                                                        + counters.meshlets * shader_constants::kMaxVerticesPerMeshlet,
                                                  .dbg_name = "meshlets_payload"});
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
