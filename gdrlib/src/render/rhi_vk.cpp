#include <volk.h>

#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_command_buffer.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/rhi_util.hpp>
#include <render/rhi_vk.hpp>

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

template<typename T, typename H>
static T vk_rhi_vkobj_from_handle(H&& handle)
{
    ZoneScoped;
    return reinterpret_cast<T>(handle.id);
}

static VkBufferUsageFlags vk_rhi_parse_buffer_usage_flags(render::rhi::buffer_usage_flags usage)
{
    VkBufferUsageFlags result = 0;
    for (u32 i = 0; i < reflection::get_enum_values_count<render::rhi::buffer_usage>(); i++)
    {
        const auto flag = reflection::get_enum_value_at<render::rhi::buffer_usage>(i);
        if (!(flag & usage))
        {
            continue;
        }

        switch (flag)
        {
        case render::rhi::buffer_usage::eCopySrc :
            result |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            break;
        case render::rhi::buffer_usage::eCopyDst :
            result |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            break;
        case render::rhi::buffer_usage::eShaderRW :
            result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        case render::rhi::buffer_usage::eIndirect :
            result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            break;
        case render::rhi::buffer_usage::eIndex :
            result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        default :
            break;
        }
    }

    return result;
}

static VkAttachmentLoadOp vk_rhi_parse_load_op(render::rhi::resource_load_op load_op)
{
    switch (load_op)
    {
    case render::rhi::resource_load_op::eLoad :
        return VK_ATTACHMENT_LOAD_OP_LOAD;
        break;
    case render::rhi::resource_load_op::eClear :
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
        break;
    default :
    case render::rhi::resource_load_op::eDiscard :
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        break;
    }
}

static VkAttachmentStoreOp vk_rhi_parse_store_op(render::rhi::resource_store_op store_op)
{
    switch (store_op)
    {
    case render::rhi::resource_store_op::eStore :
        return VK_ATTACHMENT_STORE_OP_STORE;
        break;
    default :
    case render::rhi::resource_store_op::eDiscard :
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        break;
    }
}

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

    RESULT_FORWARD_IF_FAILED(ctx);
    return create_handle<context>(*ctx);
}

void render::rhi::vk_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        render::vk_destroy_context(*ctx);
        clear_handle(ctx, context);
    }
}

auto render::rhi::vk_create_swapchain(context context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    auto sc = render::vk_create_swapchain(*ctx, desc.format, desc.size, desc.frames_in_flight, desc.vsync, nullptr);

    RESULT_FORWARD_IF_FAILED(sc);
    vk_rhi_swaphain_data rhi_sc_data;

    rhi_sc_data.sync_objects = vk_rhi_create_sc_sync(ctx->device, *sc);
    rhi_sc_data.root         = std::move(*sc);

    return create_handle<swapchain>(rhi_sc_data);
}

auto render::rhi::vk_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc)
    -> result<render::rhi::swapchain>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* sc  = cast_from_handle<vk_rhi_swaphain_data>(swapchain);
    if (!ctx || !sc)
    {
        return error("failed to access the context or the swapchain");
    }

    auto new_sc =
        render::vk_create_swapchain(*ctx, desc.format, desc.size, desc.frames_in_flight, desc.vsync, &sc->root);

    RESULT_FORWARD_IF_FAILED(new_sc);
    render::vk_destroy_swapchain(*ctx, sc->root);
    sc->root = std::move(*new_sc);

    return reference_handle<render::rhi::swapchain>(sc);
}

void render::rhi::vk_destroy_swapchain(context context, swapchain& swapchain)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);

    if (ctx && sc)
    {
        for (auto& sync : sc->sync_objects)
        {
            VK_DESTROY(sync.fence, vkDestroyFence, ctx->device);
            VK_DESTROY(sync.acquire_semaphore, vkDestroySemaphore, ctx->device);
        }

        render::vk_destroy_swapchain(*ctx, sc->root);
        clear_handle(sc, swapchain);
    }
}

auto render::rhi::vk_create_command_buffer(context context, queue_kind queue_kind) -> result<command_buffer>
{
    ZoneScoped;
    auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the device");
    }

    auto cmd = render::vk_create_command_buffer(ctx->device, ctx->queues[static_cast<u32>(queue_kind)].family);

    RESULT_FORWARD_IF_FAILED(cmd);
    return create_handle<command_buffer>(*cmd);
}

void render::rhi::vk_destroy_command_buffer(context context, command_buffer& cmd)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vkcmd     = cast_from_handle<render::vk_command_buffer>(cmd);

    if (ctx && vkcmd)
    {
        render::vk_destroy_command_buffer(ctx->device, *vkcmd);
        clear_handle(vkcmd, cmd);
    }
}

auto render::rhi::vk_create_bindless_set(context context, u32 resource_count) -> result<bindless_set>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    auto set = render::vk_create_bindless_textures_set(ctx->device, resource_count);

    RESULT_FORWARD_IF_FAILED(set);
    return create_handle<bindless_set>(*set);
}

void render::rhi::vk_destroy_bindless_set(context context, bindless_set& set)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vkset     = cast_from_handle<render::vk_descriptor_set>(set);

    if (ctx && vkset)
    {
        render::vk_destroy_descriptor_set(ctx->device, *vkset);
        clear_handle(vkset, set);
    }
}

auto render::rhi::vk_create_shader(context context, const fs::path& path) -> result<shader>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    auto vksdr = render::vk_shader::load(ctx->device, path);

    RESULT_FORWARD_IF_FAILED(vksdr);
    return create_handle<shader>(*vksdr);
}

void render::rhi::vk_destroy_shader(context context, shader& shader)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vksdr     = cast_from_handle<render::vk_shader>(shader);

    if (ctx && vksdr)
    {
        render::vk_destroy_shader(ctx->device, *vksdr);
        clear_handle(vksdr, shader);
    }
}

auto render::rhi::vk_create_buffer(context context, const create_buffer_info& buffer_info) -> result<buffer>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    auto vkbuf =
        render::vk_create_buffer(buffer_info.size,
                                 vk_rhi_parse_buffer_usage_flags(buffer_info.usage_flags),
                                 ctx->allocator,
                                 buffer_info.mapped ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);

    RESULT_FORWARD_IF_FAILED(vkbuf);

    if (buffer_info.mapped)
    {
        *buffer_info.mapped = vkbuf->mapped;
    }

    return create_handle<buffer>(*vkbuf);
}

void render::rhi::vk_destroy_buffer(context context, buffer& buffer)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vkbuf     = cast_from_handle<render::vk_buffer>(buffer);

    if (ctx && vkbuf)
    {
        render::vk_destroy_buffer(ctx->allocator, *vkbuf);
        clear_handle(vkbuf, buffer);
    }
}

auto render::rhi::vk_create_compute_pso(context context, shader shader, std::span<const bindless_set> sets)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* ctx   = cast_from_handle<render::vk_context>(context);
    const auto* vksdr = cast_from_handle<render::vk_shader>(shader);
    if (!ctx || !vksdr)
    {
        return error("failed to access the context or the shader");
    }

    vk_descriptor_set vk_desc_sets[16];

    assert2(sets.size() <= COUNT_OF(vk_desc_sets));
    for (u32 i = 0; i < cpp::min(sets.size(), COUNT_OF(vk_desc_sets)); i++)
    {
        auto* vkset = cast_from_handle<vk_descriptor_set>(sets[i]);
        if (!vkset)
        {
            return error("failed to access the descriptor set");
        }

        vk_desc_sets[i] = *vkset;
    }

    auto vkpso = render::vk_pipeline::create_compute(ctx->device, *vksdr, vk_desc_sets, sets.size());

    RESULT_FORWARD_IF_FAILED(vkpso);
    return create_handle<pipeline>(*vkpso);
}

auto render::rhi::vk_create_graphics_pso(context context, std::span<const shader> shaders,
                                         const std::span<const bindless_set> sets, const nlohmann::json& options)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    if (!ctx)
    {
        return error("failed to access the context");
    }

    vk_descriptor_set vk_desc_sets[16];
    assert2(sets.size() <= COUNT_OF(vk_desc_sets));

    for (u32 i = 0; i < cpp::min(sets.size(), COUNT_OF(vk_desc_sets)); i++)
    {
        auto* vkset = cast_from_handle<vk_descriptor_set>(sets[i]);
        if (!vkset)
        {
            return error("failed to access the descriptor set");
        }

        vk_desc_sets[i] = *vkset;
    }

    vk_shader vk_shaders[16];
    assert2(shaders.size() <= COUNT_OF(vk_shaders));

    for (u32 i = 0; i < cpp::min(shaders.size(), COUNT_OF(vk_shaders)); i++)
    {
        auto* vkset = cast_from_handle<vk_shader>(shaders[i]);
        if (!vkset)
        {
            return error("failed to access the shader");
        }

        vk_shaders[i] = *vkset;
    }

    auto vkpso = render::vk_pipeline::create_graphics(ctx->device,
                                                      vk_shaders,
                                                      shaders.size(),
                                                      VK_FORMAT_B8G8R8A8_UNORM,
                                                      VK_FORMAT_UNDEFINED,
                                                      vk_desc_sets,
                                                      sets.size(),
                                                      options);

    RESULT_FORWARD_IF_FAILED(vkpso);
    return create_handle<pipeline>(*vkpso);
}

void render::rhi::vk_destroy_pso(context context, pipeline& pso)
{
    ZoneScoped;
    const auto* ctx = cast_from_handle<render::vk_context>(context);
    auto* vkpso     = cast_from_handle<render::vk_pipeline>(pso);

    if (ctx && vkpso)
    {
        render::vk_destroy_pipeline(ctx->device, *vkpso);
        clear_handle(vkpso, pso);
    }
}

auto render::rhi::vk_query_shader_stage(shader shader) -> result<VkShaderStageFlagBits>
{
    ZoneScoped;
    if (auto* vksdr = cast_from_handle<vk_shader>(shader))
    {
        return vksdr->meta.stage;
    }

    return error("failed to access the shader");
}

auto render::rhi::vk_query_swapchain_images_count(swapchain swapchain) -> result<u32>
{
    ZoneScoped;
    if (auto* sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return sc->sync_objects.size();
    }

    return error("failed to access the swapchain");
}

auto render::rhi::vk_query_current_frame_index(swapchain swapchain) -> result<u32>
{
    ZoneScoped;
    if (auto* sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return sc->frame_index;
    }

    return error("failed to access the swapchain");
}

auto render::rhi::vk_query_queue(context context, queue_kind kind) -> result<queue>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return queue {.id = reinterpret_cast<u64>(ctx->queues[static_cast<u32>(kind)].queue)};
    }

    return error("failed to access the context");
}

auto render::rhi::vk_query_device(context context) -> result<device>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return device {.id = reinterpret_cast<u64>(ctx->device)};
    }

    return error("failed to access the context");
}

auto render::rhi::vk_query_physical_device(context context) -> result<physical_device>
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        return physical_device {.id = reinterpret_cast<u64>(ctx->physical_device)};
    }

    return error("failed to access the context");
}

void render::rhi::vk_queue_wait_idle(queue queue)
{
    ZoneScoped;
    if (auto vk_queue = vk_rhi_vkobj_from_handle<VkQueue>(queue); vk_queue != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(vk_queue);
    }
}

void render::rhi::vk_device_wait_idle(context context)
{
    ZoneScoped;
    if (auto* ctx = cast_from_handle<render::vk_context>(context))
    {
        vkDeviceWaitIdle(ctx->device);
    }
}

auto render::rhi::vk_acquire_next_swapchain_image(context context, swapchain swapchain) -> result<image>
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

void render::rhi::vk_cmd_begin_recording(command_buffer cmd)
{
    ZoneScoped;
    if (auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd))
    {
        constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
        vkBeginCommandBuffer(vkcmd->cmd_buffer, &command_buffer_begin_info);
    }
}

void render::rhi::vk_cmd_end_recording(command_buffer cmd)
{
    ZoneScoped;
    if (auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd))
    {
        vkEndCommandBuffer(vkcmd->cmd_buffer);
    }
}

void render::rhi::vk_cmd_transition_image(command_buffer cmd, image dst, image_layout dst_layout)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    auto* vkimg = cast_from_handle<render::vk_image>(dst);
    if (!vkcmd || !vkimg)
    {
        return;
    }

    const auto vkdst = dst_layout == image_layout::eCommon ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    render::vk_transition_image(vkcmd->cmd_buffer, vkimg->image, vkimg->layout, vkdst);
    vkimg->layout = vkdst;
}

void render::rhi::vk_cmd_present_image(command_buffer cmd, swapchain swapchain, queue submit, queue present)
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
        vk_rhi_vkobj_from_handle<VkQueue>(submit), 1, &gfx_submit_info, sc->sync_objects[sc->frame_index].fence));

    const VkPresentInfoKHR present_info_khr {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &sc->root.images[sc->image_index].release_semaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &sc->root.sc,
        .pImageIndices      = &sc->image_index,
        .pResults           = nullptr,
    };

    const auto present_result = vkQueuePresentKHR(vk_rhi_vkobj_from_handle<VkQueue>(present), &present_info_khr);
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

void render::rhi::vk_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                        attachment_state_info depth_attachment, uvec4 viewport)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    if (!vkcmd)
    {
        return;
    }

    VkRect2D vp = {static_cast<i32>(viewport.x), static_cast<i32>(viewport.y), viewport.z, viewport.w};
    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .renderArea = vp, .layerCount = 1};

    VkRenderingAttachmentInfo color_attachment_info[16] {};
    assert2(color_attachments.size() <= COUNT_OF(color_attachment_info));
    for (u32 i = 0; i < cpp::min(color_attachments.size(), COUNT_OF(color_attachment_info)); i++)
    {
        auto* vkimage = cast_from_handle<vk_image>(color_attachments[i].attachment);
        if (!vkimage)
        {
            return;
        }

        color_attachment_info[i] = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = vkimage->view,
            .imageLayout = vkimage->layout,
            .loadOp      = vk_rhi_parse_load_op(color_attachments[i].load_op),
            .storeOp     = vk_rhi_parse_store_op(color_attachments[i].store_op),
        };
    }

    if (!color_attachments.empty())
    {
        rendering_info.pColorAttachments    = color_attachment_info;
        rendering_info.colorAttachmentCount = color_attachments.size();
    }

    VkRenderingAttachmentInfo depth_attachment_info {};
    if (depth_attachment.attachment != null_image)
    {
        auto* vkimage = cast_from_handle<vk_image>(depth_attachment.attachment);
        if (!vkimage)
        {
            return;
        }

        depth_attachment_info = {
            .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView   = vkimage->view,
            .imageLayout = vkimage->layout,
            .loadOp      = vk_rhi_parse_load_op(depth_attachment.load_op),
            .storeOp     = vk_rhi_parse_store_op(depth_attachment.store_op),
        };

        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(vkcmd->cmd_buffer, &rendering_info);

    const VkViewport vkvp {.x        = static_cast<f32>(vp.offset.x),
                           .y        = static_cast<f32>(vp.offset.y),
                           .width    = static_cast<f32>(vp.extent.width),
                           .height   = static_cast<f32>(vp.extent.height),
                           .minDepth = 0.0F,
                           .maxDepth = 1.0F};

    vkCmdSetCullMode(vkcmd->cmd_buffer, VK_CULL_MODE_NONE);
    vkCmdSetScissor(vkcmd->cmd_buffer, 0, 1, &vp);
    vkCmdSetViewport(vkcmd->cmd_buffer, 0, 1, &vkvp);
}

void render::rhi::vk_cmd_clear_draw_state(command_buffer cmd)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    if (!vkcmd)
    {
        return;
    }

    vkCmdEndRendering(vkcmd->cmd_buffer);
}

void render::rhi::vk_cmd_bind_pso(command_buffer cmd, pipeline pso)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    auto* vkpso = cast_from_handle<render::vk_pipeline>(pso);

    if (!vkcmd || !vkpso)
    {
        return;
    }

    vkpso->bind(vkcmd->cmd_buffer);
}

void render::rhi::vk_cmd_draw_instanced(command_buffer cmd, u32 vtx_count, u32 instance_count, u32 first_vertex,
                                        u32 first_instance)
{
    ZoneScoped;
    auto* vkcmd = cast_from_handle<render::vk_command_buffer>(cmd);
    if (!vkcmd)
    {
        return;
    }

    vkCmdDraw(vkcmd->cmd_buffer, vtx_count, instance_count, first_vertex, first_instance);
}
