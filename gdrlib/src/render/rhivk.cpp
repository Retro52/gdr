#include <volk.h>

#include <render/platform/vk/vk_command_buffer.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/rhivk.hpp>

template<typename T, typename H>
static T* cast_from_handle(H&& handle)
{
    ZoneScoped;
    return reinterpret_cast<T*>(handle.id);
}

template<typename T, typename H>
static T vkobj_from_handle(H&& handle)
{
    ZoneScoped;
    return reinterpret_cast<T>(handle.id);
}

template<typename T>
static T* create(T& handle)
{
    ZoneScoped;
    return new T(std::move(handle));
}

template<typename T>
static void erase(T&& data)
{
    ZoneScoped;
    memset(&data, 0, sizeof(data));
}

struct vk_swapchain_sync_objects
{
    VkFence fence {VK_NULL_HANDLE};
    VkSemaphore acquire_semaphore {VK_NULL_HANDLE};
};

struct vk_rhi_swaphain_data
{
    u32 image_index = 0;
    u32 frame_index = 0;

    render::vk_swapchain root;
    cpp::heap_array<vk_swapchain_sync_objects> sync_objects;
};

static cpp::heap_array<vk_swapchain_sync_objects> vk_rhi_create_sc_sync(VkDevice device,
                                                                        const render::vk_swapchain& swapchain)
{
    ZoneScoped;
    constexpr VkSemaphoreCreateInfo semaphore_create_info {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo fence_create_info {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    cpp::heap_array<vk_swapchain_sync_objects> result(swapchain.images.size());
    for (auto& sync : result)
    {
        VK_ASSERT_ON_FAIL(vkCreateFence(device, &fence_create_info, nullptr, &sync.fence));
        VK_ASSERT_ON_FAIL(vkCreateSemaphore(device, &semaphore_create_info, nullptr, &sync.acquire_semaphore));
    }

    return result;
}

auto render::rhi::vk_create_context(const window& window, const instance_desc& desc) -> result<context>
{
    ZoneScoped;
    auto ctx = render::vk_create_context(window, desc);
    if (!ctx)
    {
        return error(ctx.message);
    }

    return context {.id = reinterpret_cast<u64>(create(*ctx))};
}

void render::rhi::vk_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        render::vk_destroy_context(*ctx);
        erase(context);

        delete ctx;
    }
}

auto render::rhi::vk_create_swapchain(const context& context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    VkSwapchainKHR vkold =
        desc.old_swapchain ? cast_from_handle<vk_rhi_swaphain_data>(*desc.old_swapchain)->root.sc : VK_NULL_HANDLE;

    auto sc = render::vk_create_swapchain(*ctx, desc.format, desc.size, desc.frames_in_flight, desc.vsync, vkold);
    if (!sc)
    {
        return error(sc.message);
    }

    vk_rhi_swaphain_data rhi_sc_data;

    rhi_sc_data.sync_objects = vk_rhi_create_sc_sync(ctx->device, *sc);
    rhi_sc_data.root         = std::move(*sc);

    return swapchain {.id = reinterpret_cast<u64>(create(rhi_sc_data))};
}

void render::rhi::vk_destroy_swapchain(const context& context, swapchain& swapchain)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);

    if (ctx && sc)
    {
        for (auto& sync : sc->sync_objects)
        {
            vkDestroyFence(ctx->device, sync.fence, nullptr);
            vkDestroySemaphore(ctx->device, sync.acquire_semaphore, nullptr);
        }

        render::vk_destroy_swapchain(*ctx, sc->root);
        erase(sc);
    }
}

auto render::rhi::vk_create_command_buffer(const context& context, queue_kind queue_kind) -> result<command_buffer>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the device");
    }

    auto cmd = render::vk_create_command_buffer(ctx->device, ctx->queues[static_cast<u32>(queue_kind)].family);
    if (cmd)
    {
        return command_buffer {.id = reinterpret_cast<u64>(create(*cmd))};
    }

    return error(cmd.message);
}

void render::rhi::vk_destroy_command_buffer(const context& context, command_buffer& cmd)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vkcmd     = cast_from_handle<render::vk_command_buffer>(cmd);

    if (ctx && vkcmd)
    {
        render::vk_destroy_command_buffer(ctx->device, *vkcmd);
        erase(cmd);
    }
}

auto render::rhi::vk_query_swapchain_images_count(const swapchain& swapchain) -> result<u32>
{
    if (auto* sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return sc->sync_objects.size();
    }

    return error("failed to access the swapchain");
}

auto render::rhi::vk_query_current_frame_index(const swapchain& swapchain) -> result<u32>
{
    if (auto* sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return sc->frame_index;
    }

    return error("failed to access the swapchain");
}

auto render::rhi::vk_query_queue(const context& context, queue_kind kind) -> result<queue>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return queue {.id = reinterpret_cast<u64>(ctx->queues[static_cast<u32>(kind)].queue)};
    }

    return error("failed to access the context");
}

auto render::rhi::vk_query_device(const context& context) -> result<device>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return device {.id = reinterpret_cast<u64>(ctx->device)};
    }

    return error("failed to access the context");
}

auto render::rhi::vk_query_physical_device(const context& context) -> result<physical_device>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return physical_device {.id = reinterpret_cast<u64>(ctx->physical_device)};
    }

    return error("failed to access the context");
}

void render::rhi::vk_device_wait_idle(const context& context)
{
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        vkDeviceWaitIdle(ctx->device);
    }
}

auto render::rhi::vk_acquire_next_swapchain_image(const context& context, swapchain& swapchain) -> result<image>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);

    if (!ctx || !sc)
    {
        return error("failed to access the context or the swapchain data");
    }

    vkWaitForFences(ctx->device, 1, &sc->sync_objects[sc->frame_index].fence, VK_TRUE, UINT64_MAX);

    u32 new_image_index       = 0;
    const auto acquire_result = vkAcquireNextImageKHR(ctx->device,
                                                      sc->root.sc,
                                                      UINT64_MAX,
                                                      sc->sync_objects[sc->frame_index].acquire_semaphore,
                                                      VK_NULL_HANDLE,
                                                      &new_image_index);

    sc->image_index = new_image_index & 0xFF;
    switch (acquire_result)
    {
    case VK_SUCCESS :
    case VK_SUBOPTIMAL_KHR :
        vkResetFences(ctx->device, 1, &sc->sync_objects[sc->frame_index].fence);
        return image {.id = reinterpret_cast<u64>(&sc->root.images[sc->image_index].image)};
    case VK_ERROR_OUT_OF_DATE_KHR :
        return error("failed to acquire swapchain image (out of date)");
    default :
        return error("failed to acquire swapchain image (unknown result)");
    }
}

void render::rhi::vk_cmd_begin_recording(command_buffer& cmd)
{
    ZoneScoped;
    if (auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd))
    {
        constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
        vkBeginCommandBuffer(vkcmd->cmd_buffer, &command_buffer_begin_info);
    }
}

void render::rhi::vk_cmd_end_recording(command_buffer& cmd)
{
    ZoneScoped;
    if (auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd))
    {
        vkEndCommandBuffer(vkcmd->cmd_buffer);
    }
}

void render::rhi::vk_cmd_transition_image(command_buffer& cmd, image& dst, image_layout dst_layout)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    auto* vkimg = cast_from_handle<render::vk_image>(dst);
    if (!vkcmd || !vkimg)
    {
        return;
    }

    const auto vkdst = dst_layout == image_layout::eCommon ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    render::transition_image(vkcmd->cmd_buffer, vkimg->image, vkimg->layout, vkdst);
    vkimg->layout = vkdst;
}

void render::rhi::vk_cmd_present_image(command_buffer& cmd, swapchain& swapchain, queue& submit, queue& present)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    auto* sc    = cast_from_handle<vk_rhi_swaphain_data>(swapchain);
    if (!vkcmd || !sc)
    {
        return;
    }

    const VkSemaphoreSubmitInfo wait_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = sc->sync_objects[sc->frame_index].acquire_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    const VkSemaphoreSubmitInfo signal_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = sc->root.images[sc->image_index].release_semaphore,
    };

    const VkCommandBufferSubmitInfo command_buffer_submit_info {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = vkcmd->cmd_buffer,
    };

    const VkSubmitInfo2 gfx_submit_info {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos    = &wait_semaphore_info,

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos    = &command_buffer_submit_info,

        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signal_semaphore_info,
    };

    VK_ASSERT_ON_FAIL(vkQueueSubmit2(
        vkobj_from_handle<VkQueue>(submit), 1, &gfx_submit_info, sc->sync_objects[sc->frame_index].fence));

    const VkPresentInfoKHR present_info_khr {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &sc->root.images[sc->image_index].release_semaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &sc->root.sc,
        .pImageIndices      = &sc->image_index,
        .pResults           = nullptr,
    };

    const auto present_result = vkQueuePresentKHR(vkobj_from_handle<VkQueue>(present), &present_info_khr);
    switch (present_result)
    {
    case VK_SUBOPTIMAL_KHR :
    case VK_ERROR_OUT_OF_DATE_KHR :
        return;
    default :
        VK_ASSERT_ON_FAIL(present_result);
    }

    sc->frame_index = (sc->frame_index + 1) % sc->sync_objects.size();
}
