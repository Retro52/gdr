#include <app/gpu_upload_mgr.hpp>
#include <app/render.hpp>
#include <cpp/containers/local_array.hpp>

static u32 gpu_upload_mgr_get_offset(const u32 width, const u32 height, const u32 block_size, const u32 bits_per_block)
{
    if (block_size > 1)
    {
        return ((width + block_size - 1) / block_size) * ((height + block_size - 1) / block_size) * bits_per_block / 8;
    }

    return width * height * bits_per_block / 8;
}

static app::mapped_buffer gpu_upload_mgr_create_mapped_buffer(rhi::impl& impl, const rhi::context ctx,
                                                              const u64 staging_buffer_size)
{
    return app::mapped_buffer::create(impl,
                                      ctx,
                                      {
                                          .size        = staging_buffer_size,
                                          .usage_flags = rhi::buffer_usage::copy_src,
                                      });
}

app::gpu_upload_mgr::gpu_upload_mgr(rhi::impl& impl, const rhi::context ctx, const u64 staging_buffer_size)
    : m_rhi(impl)
    , m_ctx(ctx)
    , m_staging_buffer(gpu_upload_mgr_create_mapped_buffer(impl, ctx, staging_buffer_size))
{
    m_upload_queue = *m_rhi.query_queue(ctx, rhi::queue_kind::transfer);
    m_staging_cmd  = *m_rhi.create_command_buffer(m_ctx, rhi::queue_kind::transfer);
}

rhi::image app::gpu_upload_mgr::create_texture(const loader::texture_desc& texture) const
{
    const rhi::create_image_info image_info {
        .format      = texture.format,
        .mips_count  = texture.mips_count,
        .layer_count = texture.arrays_count,
        .dimensions  = texture.dimensions,
        .usage_flags = rhi::image_usage::transfer_dst | rhi::image_usage::sampled,
    };

    const auto image = *m_rhi.create_image(m_ctx, image_info);
    std::memcpy(m_staging_buffer.mapped, texture.pdata.data(), texture.pdata.size());

    const auto cmd = m_staging_cmd;
    m_rhi.cmd_reset(cmd);
    m_rhi.cmd_begin_recording(cmd);

    const auto barrier = app::make_image_barrier(image,
                                                 rhi::image_layout::discard,
                                                 rhi::image_layout::common,
                                                 rhi::barrier_stage::none,
                                                 rhi::barrier_stage::copy,
                                                 rhi::barrier_access::none,
                                                 rhi::barrier_access::copy_read,
                                                 rhi::image_aspect::color);

    rhi::image_barrier barriers[] = {barrier};

    m_rhi.cmd_barriers(cmd, rhi::barrier_batch {.images = std::span {barriers}});

    u64 src_offset = 0;
    u32 mip_w      = texture.dimensions.x;
    u32 mip_h      = texture.dimensions.y;

    cpp::local_array<rhi::copy_image_info> regions(texture.mips_count);
    for (unsigned int i = 0; i < texture.mips_count; ++i)
    {
        regions[i] = {
            .source_offset = src_offset,

            .dst_mip    = i,
            .dst_extent = uvec3(mip_w, mip_h, 1),
        };

        src_offset += gpu_upload_mgr_get_offset(mip_w, mip_h, texture.block_size, texture.bits_per_block);

        mip_w = mip_w > 1 ? mip_w / 2 : 1;
        mip_h = mip_h > 1 ? mip_h / 2 : 1;
    }

    m_rhi.cmd_copy_buffer_to_image(
        cmd, m_staging_buffer.buffer, image, rhi::image_layout::common, std::span {regions.data(), regions.size()});
    m_rhi.cmd_end_recording(cmd);

    auto fence = *m_rhi.create_fence(m_ctx, 0);

    const rhi::command_buffer command_buffers[]  = {m_staging_cmd};
    const rhi::fence_submit_info signal_fences[] = {
        {.fence = fence, .value = 1}
    };

    m_rhi.submit(
        m_ctx, m_upload_queue, {.signals = std::span {signal_fences}, .command_buffers = std::span {command_buffers}});

    m_rhi.fence_wait_for_value(m_ctx, fence, 1);
    m_rhi.destroy_fence(m_ctx, fence);

    return image;
}

void app::gpu_upload_mgr::submit_internal(const rhi::buffer dst, const u64 dst_offset, const void* data,
                                          u64 bytes) const
{
    cpp::cx_memcpy(m_staging_buffer.mapped, data, bytes);

    const auto cmd = m_staging_cmd;
    m_rhi.cmd_reset(cmd);
    m_rhi.cmd_begin_recording(cmd);
    m_rhi.cmd_copy_buffer(cmd, m_staging_buffer.buffer, {0, bytes}, dst, dst_offset);
    m_rhi.cmd_end_recording(cmd);

    auto fence = *m_rhi.create_fence(m_ctx, 0);

    const rhi::command_buffer command_buffers[]  = {m_staging_cmd};
    const rhi::fence_submit_info signal_fences[] = {
        {.fence = fence, .value = 1}
    };

    m_rhi.submit(
        m_ctx, m_upload_queue, {.signals = std::span {signal_fences}, .command_buffers = std::span {command_buffers}});

    m_rhi.fence_wait_for_value(m_ctx, fence, 1);
    m_rhi.destroy_fence(m_ctx, fence);
}
