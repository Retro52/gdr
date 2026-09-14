#pragma once

#include <pod_types.hpp>

#define RHI_REGISTER_HANDLE(NAME) using NAME = handle<struct NAME##_tag>

namespace render::rhi
{
    template<typename>
    struct handle
    {
        u64 id;
    };

    RHI_REGISTER_HANDLE(device);
    RHI_REGISTER_HANDLE(physical_device);
    RHI_REGISTER_HANDLE(sampler);
    RHI_REGISTER_HANDLE(context);
    RHI_REGISTER_HANDLE(queue);
    RHI_REGISTER_HANDLE(image);
    RHI_REGISTER_HANDLE(buffer);
    RHI_REGISTER_HANDLE(surface);
    RHI_REGISTER_HANDLE(pipeline);
    RHI_REGISTER_HANDLE(shader);
    RHI_REGISTER_HANDLE(bindless_set);
    RHI_REGISTER_HANDLE(swapchain);
    RHI_REGISTER_HANDLE(command_buffer);
}
