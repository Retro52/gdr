#include <app/gpu_upload_mgr.hpp>

static app::mapped_buffer gpu_upload_mgr_create_mapped_buffer(render::rhi::rhi& rhi, const render::rhi::context ctx,
                                                              const u64 staging_buffer_size)
{
    return app::mapped_buffer::create(rhi,
                                      ctx,
                                      {
                                          .size        = staging_buffer_size,
                                          .usage_flags = static_cast<u32>(render::rhi::buffer_usage::copy_src),
                                      });
}

app::gpu_upload_mgr::gpu_upload_mgr(render::rhi::rhi& rhi, const render::rhi::context ctx,
                                    const u64 staging_buffer_size)
    : m_rhi(rhi)
    , m_ctx(ctx)
    , m_staging_buffer(gpu_upload_mgr_create_mapped_buffer(rhi, ctx, staging_buffer_size))
{
    m_upload_queue = *m_rhi.query_queue(ctx, render::rhi::queue_kind::transfer);
    m_staging_cmd  = *m_rhi.create_command_buffer(m_ctx, render::rhi::queue_kind::transfer);
}

void app::gpu_upload_mgr::submit_internal(const render::rhi::buffer dst, const u64 dst_offset, const void* data,
                                          u64 bytes) const
{
    cpp::cx_memcpy(m_staging_buffer.mapped, data, bytes);

    const auto cmd = m_staging_cmd;
    m_rhi.cmd_reset(cmd);
    m_rhi.cmd_begin_recording(cmd);
    m_rhi.cmd_copy_buffer(cmd, m_staging_buffer.buffer, {0, bytes}, dst, dst_offset);
    m_rhi.cmd_end_recording(cmd);

    auto fence = *m_rhi.create_fence(m_ctx, 0);

    const render::rhi::command_buffer command_buffers[]  = {m_staging_cmd};
    const render::rhi::fence_submit_info signal_fences[] = {
        {.fence = fence, .value = 1}
    };

    m_rhi.submit(
        m_ctx, m_upload_queue, {.signals = std::span {signal_fences}, .command_buffers = std::span {command_buffers}});

    m_rhi.fence_wait_for_value(m_ctx, fence, 1);
    m_rhi.destroy_fence(m_ctx, fence);
}
