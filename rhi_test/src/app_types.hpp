#pragma once

#include <render/rhi.hpp>

struct buffer_transfer
{
    void* mapped;
    render::rhi::queue queue;
    render::rhi::buffer staging_buffer;
    render::rhi::command_buffer staging_command_buffer;
};

struct shared_buffer
{
    u64 size {0};
    u64 offset {0};
    render::rhi::buffer buffer;

    shared_buffer() = default;

    explicit shared_buffer(const render::rhi::rhi& rhi, render::rhi::context ctx, const u64 size,
                           render::rhi::buffer_usage_flags usage)
        : size(size)
        , offset(0)
    {
        render::rhi::create_buffer_info cbi {.size = size, .usage_flags = render::rhi::buffer_usage::eCopyDst | usage};
        buffer = *rhi.create_buffer(ctx, cbi);
    }

    explicit shared_buffer(const render::rhi::rhi& rhi, render::rhi::context ctx, const u64 size,
                           render::rhi::buffer_usage usage)
        : shared_buffer(rhi, ctx, size, static_cast<render::rhi::buffer_usage_flags>(usage))
    {
    }
};

struct scene_geometry_pool
{
    shared_buffer vertex;
    shared_buffer meshlets;
    shared_buffer primitives;
    shared_buffer instances;
    shared_buffer materials;
    shared_buffer meshlets_payload;

    buffer_transfer transfer;
};
