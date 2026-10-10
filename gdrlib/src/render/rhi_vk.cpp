#include <volk.h>

#include <cpp/containers/local_array.hpp>
#include <render/platform/vk/vk_buffer.hpp>
#include <render/platform/vk/vk_command_buffer.hpp>
#include <render/platform/vk/vk_descriptor_set.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_timeline_semaphore.hpp>
#include <render/platform/vk/vk_utils.hpp>
#include <render/rhi_util.hpp>
#include <render/rhi_vk.hpp>

#include "entt/core/any.hpp"

struct vk_swapchain_sync_objects
{
    VkFence fence {VK_NULL_HANDLE};
    VkSemaphore acquire_semaphore {VK_NULL_HANDLE};
};

struct vk_rhi_swaphain_data
{
    u32 image_index = 0;
    u32 frame_index = 0;

    platform::vk_swapchain root;
    cpp::heap_array<vk_swapchain_sync_objects> sync_objects;
};

struct vk_rhi_image_view
{
    const platform::vk_image& source;
    VkImageView view = VK_NULL_HANDLE;
};

struct vk_rhi_attachment_objects
{
    VkImageView view     = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

template<typename T>
static void vk_rhi_set_object_name_if_needed(VkDevice device, T object, const char* name)
{
    if (!name || object == VK_NULL_HANDLE || !vkSetDebugUtilsObjectNameEXT)
    {
        return;
    }

    VkObjectType type = VK_OBJECT_TYPE_UNKNOWN;
    if constexpr (std::is_same_v<T, VkImage>)
    {
        type = VK_OBJECT_TYPE_IMAGE;
    }
    else if constexpr (std::is_same_v<T, VkBuffer>)
    {
        type = VK_OBJECT_TYPE_BUFFER;
    }
    else if constexpr (std::is_same_v<T, VkPipeline>)
    {
        type = VK_OBJECT_TYPE_PIPELINE;
    }
    else if constexpr (std::is_same_v<T, VkSampler>)
    {
        type = VK_OBJECT_TYPE_SAMPLER;
    }
    else if constexpr (std::is_same_v<T, VkImageView>)
    {
        type = VK_OBJECT_TYPE_IMAGE_VIEW;
    }
    else
    {
        static_assert(sizeof(T) == 0, "unsupported object type");
    }

    const VkDebugUtilsObjectNameInfoEXT info {
        .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType   = type,
        .objectHandle = reinterpret_cast<u64>(object),
        .pObjectName  = name,
    };

    vkSetDebugUtilsObjectNameEXT(device, &info);
}

template<typename T, typename H>
static T vk_rhi_vkobj_from_handle(H&& handle)
{
    ZoneScoped;
    return reinterpret_cast<T>(handle.id);
}

static VkMemoryBarrier2 vk_rhi_parse_global_barrier(const rhi::global_barrier global_barrier)
{
    return VkMemoryBarrier2 {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .pNext         = nullptr,
        .srcStageMask  = platform::vk_rhi_parse_stage_flags(global_barrier.before.stages),
        .srcAccessMask = platform::vk_rhi_parse_access_flags(global_barrier.before.access),
        .dstStageMask  = platform::vk_rhi_parse_stage_flags(global_barrier.after.stages),
        .dstAccessMask = platform::vk_rhi_parse_access_flags(global_barrier.after.access),
    };
}

static VkImageMemoryBarrier2 vk_rhi_parse_image_barrier(const rhi::image_barrier& image_barrier)
{
    auto* vk_image = rhi::cast_from_handle<platform::vk_image>(image_barrier.image);
    if (!vk_image)
    {
        return {};
    }

    const auto new_layout = image_barrier.layout_after == rhi::image_layout::current
                              ? vk_image->layout
                              : platform::vk_rhi_parse_image_layout(image_barrier.layout_after);
    const auto range      = platform::vk_image_subresource_range(
        platform::vk_rhi_parse_aspect_flags(image_barrier.range.aspects),
        image_barrier.range.mips_range.x,
        image_barrier.range.mips_range.y == rhi::kMipsAll ? VK_REMAINING_MIP_LEVELS : image_barrier.range.mips_range.y,
        image_barrier.range.layers_range.x,
        image_barrier.range.layers_range.y == rhi::kLayersAll ? VK_REMAINING_ARRAY_LAYERS
                                                                   : image_barrier.range.layers_range.y);
    const VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask  = platform::vk_rhi_parse_stage_flags(image_barrier.before.stages),
        .srcAccessMask = platform::vk_rhi_parse_access_flags(image_barrier.before.access),
        .dstStageMask  = platform::vk_rhi_parse_stage_flags(image_barrier.after.stages),
        .dstAccessMask = platform::vk_rhi_parse_access_flags(image_barrier.after.access),

        .oldLayout = image_barrier.layout_before == rhi::image_layout::current
                       ? vk_image->layout
                       : platform::vk_rhi_parse_image_layout(image_barrier.layout_before),
        .newLayout = new_layout,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image            = vk_image->image,
        .subresourceRange = range,
    };

    vk_image->layout = new_layout;
    return barrier;
}

static VkBufferMemoryBarrier2 vk_rhi_parse_buffer_barrier(rhi::buffer_barrier buffer_barrier)
{
    auto* vk_buffer = rhi::cast_from_handle<platform::vk_buffer>(buffer_barrier.buffer);
    if (!vk_buffer)
    {
        return {};
    }

    return VkBufferMemoryBarrier2 {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask        = platform::vk_rhi_parse_stage_flags(buffer_barrier.before.stages),
        .srcAccessMask       = platform::vk_rhi_parse_access_flags(buffer_barrier.before.access),
        .dstStageMask        = platform::vk_rhi_parse_stage_flags(buffer_barrier.after.stages),
        .dstAccessMask       = platform::vk_rhi_parse_access_flags(buffer_barrier.after.access),
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer              = vk_buffer->buffer,
        .offset              = buffer_barrier.data_range.x,
        .size = buffer_barrier.data_range.y == rhi::kBufferAll ? VK_WHOLE_SIZE : buffer_barrier.data_range.y,
    };
}

static cpp::heap_array<vk_swapchain_sync_objects> vk_rhi_create_sc_sync(VkDevice device,
                                                                        const platform::vk_swapchain& swapchain)
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

static result<vk_rhi_attachment_objects> vk_rhi_parse_attachment_objects(const rhi::attachment attachment)
{
    vk_rhi_attachment_objects result;

    if (attachment.is_image_view())
    {
        auto* vk_view = cast_from_handle<vk_rhi_image_view>(attachment.get_image_view());
        if (!vk_view)
        {
            return error("one of views is null");
        }

        result.view   = vk_view->view;
        result.layout = vk_view->source.layout;
    }
    else
    {
        auto* vk_image = cast_from_handle<platform::vk_image>(attachment.get_image());
        if (!vk_image)
        {
            return error("one of images is null");
        }

        result.view   = vk_image->view;
        result.layout = vk_image->layout;
    }

    return result;
}

static result<VkRenderingAttachmentInfo> vk_rhi_parse_attachment_info(const rhi::attachment_state_info& info,
                                                                      const bool is_depth)
{
    auto data = vk_rhi_parse_attachment_objects(info.attachment);
    RESULT_FORWARD_IF_FAILED(data);

    return VkRenderingAttachmentInfo {
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = data->view,
        .imageLayout = data->layout,
        .loadOp      = platform::vk_rhi_parse_load_op(info.load_op),
        .storeOp     = platform::vk_rhi_parse_store_op(info.store_op),
        .clearValue  = is_depth ? platform::vk_rhi_parse_depth_clear_value(info.clear_value)
                                : platform::vk_rhi_parse_color_clear_value(info.clear_value),
    };
}

static platform::vk_descriptor_info vk_rhi_parse_descriptor_info(const rhi::binding binding)
{
    if (binding.has_buffer())
    {
        auto* vk_buffer = rhi::cast_from_handle<platform::vk_buffer>(binding.get_buffer());
        if (!vk_buffer)
        {
            return VK_NULL_HANDLE;
        }

        return vk_buffer->buffer;
    }

    platform::vk_descriptor_info result {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    if (binding.has_sampler())
    {
        result.m_image.sampler = vk_rhi_vkobj_from_handle<VkSampler>(binding.get_buffer());
    }

    if (binding.has_attachment())
    {
        auto data = vk_rhi_parse_attachment_objects(binding.get_attachment());
        if (!data)
        {
            return result;
        }

        result.m_image.imageView   = data->view;
        result.m_image.imageLayout = data->layout;
    }

    return result;
}

auto rhi::vk_create_context(const window& window, const instance_desc& desc) -> result<context>
{
    ZoneScoped;
    auto ctx = platform::vk_create_context(window, desc);

    RESULT_FORWARD_IF_FAILED(ctx);
    return create_handle<context>(*ctx);
}

void rhi::vk_destroy_context(context& context)
{
    ZoneScoped;
    if (auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        platform::vk_destroy_context(*vk_ctx);
        clear_handle(vk_ctx, context);
    }
}

auto rhi::vk_create_swapchain(context context, const create_swapchain_info& desc) -> result<swapchain>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto vk_sc = platform::vk_create_swapchain(
        *vk_ctx, static_cast<VkFormat>(desc.format), desc.size, desc.frames_in_flight, desc.vsync, nullptr);

    RESULT_FORWARD_IF_FAILED(vk_sc);
    vk_rhi_swaphain_data rhi_sc_data;

    // TODO: mb move within the vk_swapchain, like in d3d12?
    rhi_sc_data.sync_objects = vk_rhi_create_sc_sync(vk_ctx->device, *vk_sc);
    rhi_sc_data.root         = std::move(*vk_sc);

    return create_handle<swapchain>(rhi_sc_data);
}

auto rhi::vk_resize_swapchain(context context, swapchain swapchain, const create_swapchain_info& desc)
    -> result<rhi::swapchain>
{
    ZoneScoped;
    auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_sc  = cast_from_handle<vk_rhi_swaphain_data>(swapchain);
    if (!vk_ctx || !vk_sc)
    {
        return error("failed to access the context or the swapchain");
    }

    auto new_sc = platform::vk_create_swapchain(
        *vk_ctx, static_cast<VkFormat>(desc.format), desc.size, desc.frames_in_flight, desc.vsync, &vk_sc->root);

    RESULT_FORWARD_IF_FAILED(new_sc);
    platform::vk_destroy_swapchain(*vk_ctx, vk_sc->root);
    vk_sc->root = std::move(*new_sc);

    return reference_handle<rhi::swapchain>(vk_sc);
}

void rhi::vk_destroy_swapchain(context context, swapchain& swapchain)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);

    if (vk_ctx && vk_sc)
    {
        for (auto& sync : vk_sc->sync_objects)
        {
            VK_DESTROY(sync.fence, vkDestroyFence, vk_ctx->device);
            VK_DESTROY(sync.acquire_semaphore, vkDestroySemaphore, vk_ctx->device);
        }

        platform::vk_destroy_swapchain(*vk_ctx, vk_sc->root);
        clear_handle(vk_sc, swapchain);
    }
}

auto rhi::vk_create_command_buffer(context context, queue_kind queue_kind) -> result<command_buffer>
{
    ZoneScoped;
    auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the device");
    }

    auto cmd = platform::vk_create_command_buffer(vk_ctx->device, vk_ctx->queues[static_cast<u32>(queue_kind)].family);

    RESULT_FORWARD_IF_FAILED(cmd);
    return create_handle<command_buffer>(*cmd);
}

void rhi::vk_destroy_command_buffer(context context, command_buffer& cmd)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_cmd       = cast_from_handle<platform::vk_command_buffer>(cmd);

    if (vk_ctx && vk_cmd)
    {
        platform::vk_destroy_command_buffer(vk_ctx->device, *vk_cmd);
        clear_handle(vk_cmd, cmd);
    }
}

auto rhi::vk_create_bindless_set(context context, u32 resource_count) -> result<bindless_set>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto set = platform::vk_create_bindless_textures_set(vk_ctx->device, resource_count);

    RESULT_FORWARD_IF_FAILED(set);
    return create_handle<bindless_set>(*set);
}

void rhi::vk_update_bindless_set(context context, bindless_set set, const std::span<const bindless_set_write_info> info)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    const auto* vk_set = cast_from_handle<platform::vk_descriptor_set>(set);

    assert2(vk_ctx && vk_set);
    if (!(vk_ctx && vk_set))
    {
        return;
    }

    struct vk_aggregated_info
    {
        VkDescriptorImageInfo img_info;
        VkWriteDescriptorSet desc_write;
    };

    cpp::local_array<VkDescriptorImageInfo> vk_infos(info.size());
    cpp::local_array<VkWriteDescriptorSet> vk_writes(info.size());

    for (u32 i = 0; i < vk_infos.size(); i++)
    {
        auto vk_attachment = vk_rhi_parse_attachment_objects(info[i].dst);
        if (!vk_attachment)
        {
            return;
        }

        vk_infos[i] = {.imageView = vk_attachment->view, .imageLayout = vk_attachment->layout};

        vk_writes[i] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = vk_set->descriptor_set,
            .dstBinding      = 0,
            .dstArrayElement = info[i].index,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo      = &vk_infos[i],
        };
    }

    vkUpdateDescriptorSets(vk_ctx->device, vk_writes.size(), vk_writes.data(), 0, nullptr);
}

void rhi::vk_destroy_bindless_set(context context, bindless_set& set)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_set       = cast_from_handle<platform::vk_descriptor_set>(set);

    if (vk_ctx && vk_set)
    {
        platform::vk_destroy_descriptor_set(vk_ctx->device, *vk_set);
        clear_handle(vk_set, set);
    }
}

auto rhi::vk_create_fence(context context, const u64 initial_value) -> result<fence>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto vk_fence = platform::vk_create_timeline_semaphore(vk_ctx->device, initial_value);

    RESULT_FORWARD_IF_FAILED(vk_fence);
    return create_handle<fence>(*vk_fence);
}

void rhi::vk_destroy_fence(context context, fence& fence)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_fence     = cast_from_handle<platform::vk_timeline_semaphore>(fence);
    if (vk_ctx && vk_fence)
    {
        platform::vk_destroy_timeline_semaphore(vk_ctx->device, *vk_fence);
        clear_handle(vk_fence, fence);
    }
}

auto rhi::vk_create_shader(context context, const fs::path& path) -> result<shader>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto vk_shader = platform::vk_shader::load(vk_ctx->device, path);

    RESULT_FORWARD_IF_FAILED(vk_shader);
    return create_handle<shader>(*vk_shader);
}

void rhi::vk_destroy_shader(context context, shader& shader)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_shader    = cast_from_handle<platform::vk_shader>(shader);

    if (vk_ctx && vk_shader)
    {
        platform::vk_destroy_shader(vk_ctx->device, *vk_shader);
        clear_handle(vk_shader, shader);
    }
}

auto rhi::vk_create_buffer(context context, const create_buffer_info& buffer_info) -> result<buffer>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto vk_buffer =
        platform::vk_create_buffer(buffer_info.size,
                                   platform::vk_rhi_parse_buffer_usage_flags(buffer_info.usage_flags),
                                   vk_ctx->allocator,
                                   buffer_info.mapped ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);

    RESULT_FORWARD_IF_FAILED(vk_buffer);

    if (buffer_info.mapped)
    {
        *buffer_info.mapped = vk_buffer->mapped;
    }

    vk_rhi_set_object_name_if_needed(vk_ctx->device, vk_buffer->buffer, buffer_info.dbg_name);
    return create_handle<buffer>(*vk_buffer);
}

void rhi::vk_destroy_buffer(context context, buffer& buffer)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_buffer    = cast_from_handle<platform::vk_buffer>(buffer);

    if (vk_ctx && vk_buffer)
    {
        platform::vk_destroy_buffer(vk_ctx->allocator, *vk_buffer);
        clear_handle(vk_buffer, buffer);
    }
}

auto rhi::vk_create_image(context context, const create_image_info& image_info) -> result<image>
{
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    const VkExtent3D extent {
        .width  = image_info.dimensions.x,
        .height = image_info.dimensions.y,
        .depth  = image_info.dimensions.z,
    };

    const VkImageCreateInfo image_create_info = {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags         = image_info.layer_count == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0U,
        .imageType     = image_info.dimensions.z > 1 ? VK_IMAGE_TYPE_3D
                       : image_info.dimensions.y > 1 ? VK_IMAGE_TYPE_2D
                                                     : VK_IMAGE_TYPE_1D,
        .format        = static_cast<VkFormat>(image_info.format),
        .extent        = extent,
        .mipLevels     = image_info.mips_count,
        .arrayLayers   = image_info.layer_count,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = platform::vk_rhi_parse_image_usage_flags(image_info.usage_flags),
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    const auto vk_image =
        platform::vk_create_image(vk_ctx->device,
                                  image_create_info,
                                  platform::vk_rhi_parse_format_aspect_flags(static_cast<VkFormat>(image_info.format)),
                                  vk_ctx->allocator);
    RESULT_FORWARD_IF_FAILED(vk_image);

    vk_rhi_set_object_name_if_needed(vk_ctx->device, vk_image->image, image_info.dbg_name);
    vk_rhi_set_object_name_if_needed(vk_ctx->device, vk_image->view, image_info.dbg_name);
    return create_handle<image>(*vk_image);
}

void rhi::vk_destroy_image(context context, image& image)
{
    const auto* vk_ctx   = cast_from_handle<platform::vk_context>(context);
    const auto* vk_image = cast_from_handle<platform::vk_image>(image);

    if (vk_ctx && vk_image)
    {
        platform::vk_destroy_image(vk_ctx->device, vk_ctx->allocator, *vk_image);
    }
}

auto rhi::vk_create_image_view(context context, image source, const create_image_view_info& view_info)
    -> result<image_view>
{
    const auto* vk_ctx   = cast_from_handle<platform::vk_context>(context);
    const auto* vk_image = cast_from_handle<platform::vk_image>(source);
    if (!vk_ctx || !vk_image)
    {
        return error("failed to access the context or the image");
    }

    const auto range = platform::vk_image_subresource_range(
        view_info.range.aspects & rhi::image_aspect::format
            ? platform::vk_rhi_parse_format_aspect_flags(static_cast<VkFormat>(view_info.format))
            : platform::vk_rhi_parse_aspect_flags(view_info.range.aspects),

        view_info.range.mips_range.x,
        view_info.range.mips_range.y == rhi::kMipsAll ? VK_REMAINING_MIP_LEVELS : view_info.range.mips_range.y,

        view_info.range.layers_range.x,
        view_info.range.layers_range.y == rhi::kLayersAll ? VK_REMAINING_ARRAY_LAYERS : view_info.range.layers_range.y);

    const VkImageViewCreateInfo image_view_create_info {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext            = nullptr,
        .image            = vk_image->image,
        .viewType         = platform::vk_rhi_parse_image_view_type(view_info.kind),
        .format           = static_cast<VkFormat>(view_info.format),
        .subresourceRange = range,
    };

    VkImageView vk_view;
    VK_RETURN_ON_FAIL(vkCreateImageView(vk_ctx->device, &image_view_create_info, nullptr, &vk_view));

    vk_rhi_set_object_name_if_needed(vk_ctx->device, vk_image->image, view_info.dbg_name);
    return create_handle<image_view>(vk_rhi_image_view {.source = *vk_image, .view = vk_view});
}

void rhi::vk_destroy_image_view(context context, image_view& view)
{
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_view      = cast_from_handle<vk_rhi_image_view>(view);
    if (vk_ctx && vk_view)
    {
        VK_DESTROY(vk_view->view, vkDestroyImageView, vk_ctx->device);
        clear_handle(vk_view, view);
    }
}

auto rhi::vk_create_sampler(context context, const create_sampler_info& sampler_info) -> result<sampler>
{
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    auto vk_sampler =
        platform::vk_create_sampler(vk_ctx->device,
                                    platform::vk_rhi_parse_sampler_filter(sampler_info.filter),
                                    platform::vk_rhi_parse_sampler_mipmap_mode(sampler_info.mipmap_mode),
                                    platform::vk_rhi_parse_sampler_address_mode(sampler_info.address_mode),
                                    platform::vk_rhi_parse_sampler_reduction(sampler_info.reduction),
                                    static_cast<f32>(sampler_info.anisotropy_factor),
                                    platform::vk_rhi_parse_compare_op(sampler_info.compare_op),
                                    platform::vk_rhi_parse_sampler_border_color(sampler_info.border_color));
    RESULT_FORWARD_IF_FAILED(vk_sampler);

    vk_rhi_set_object_name_if_needed(vk_ctx->device, *vk_sampler, sampler_info.dbg_name);
    return sampler {.id = reinterpret_cast<u64>(*vk_sampler)};
}

void rhi::vk_destroy_sampler(context context, sampler& sampler)
{
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto vk_sampler    = vk_rhi_vkobj_from_handle<VkSampler>(sampler);

    assert2(vk_ctx && vk_sampler != VK_NULL_HANDLE);
    if (vk_ctx && vk_sampler != VK_NULL_HANDLE)
    {
        VK_DESTROY(vk_sampler, vkDestroySampler, vk_ctx->device);
        clear_handle(nullptr, sampler);
    }
}

auto rhi::vk_create_compute_pso(context context, shader shader, const std::span<const bindless_set> sets)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* vk_ctx    = cast_from_handle<platform::vk_context>(context);
    const auto* vk_shader = cast_from_handle<platform::vk_shader>(shader);
    if (!vk_ctx || !vk_shader)
    {
        return error("failed to access the context or the shader");
    }

    platform::vk_descriptor_set vk_desc_sets[16];

    assert2(sets.size() <= COUNT_OF(vk_desc_sets));
    for (u32 i = 0; i < cpp::min(sets.size(), COUNT_OF(vk_desc_sets)); i++)
    {
        const auto* vk_set = cast_from_handle<platform::vk_descriptor_set>(sets[i]);
        if (!vk_set)
        {
            return error("failed to access the descriptor set");
        }

        vk_desc_sets[i] = *vk_set;
    }

    auto vk_pso = platform::vk_pipeline::create_compute(vk_ctx->device, *vk_shader, vk_desc_sets, sets.size());

    RESULT_FORWARD_IF_FAILED(vk_pso);
    return create_handle<pipeline>(*vk_pso);
}

auto rhi::vk_create_graphics_pso(context context, const std::span<const shader> shaders,
                                 const std::span<const bindless_set> sets, const pso_options& options)
    -> result<pipeline>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    if (!vk_ctx)
    {
        return error("failed to access the context");
    }

    cpp::local_array<platform::vk_descriptor_set> vk_desc_sets(sets.size());
    for (u32 i = 0; i < vk_desc_sets.size(); i++)
    {
        const auto* vk_set = cast_from_handle<platform::vk_descriptor_set>(sets[i]);
        if (!vk_set)
        {
            return error("failed to access the descriptor set");
        }

        vk_desc_sets[i] = *vk_set;
    }

    cpp::local_array<platform::vk_shader> vk_shaders(shaders.size());
    for (u32 i = 0; i < vk_shaders.size(); i++)
    {
        const auto* vk_shader = cast_from_handle<platform::vk_shader>(shaders[i]);
        if (!vk_shader)
        {
            return error("failed to access the shader");
        }

        vk_shaders[i] = *vk_shader;
    }

    auto vk_pso = platform::vk_pipeline::create_graphics(
        vk_ctx->device, vk_shaders.data(), vk_shaders.size(), vk_desc_sets.data(), vk_desc_sets.size(), options);

    RESULT_FORWARD_IF_FAILED(vk_pso);
    return create_handle<pipeline>(*vk_pso);
}

void rhi::vk_destroy_pso(context context, pipeline& pso)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_pso       = cast_from_handle<platform::vk_pipeline>(pso);

    if (vk_ctx && vk_pso)
    {
        platform::vk_destroy_pipeline(vk_ctx->device, *vk_pso);
        clear_handle(vk_pso, pso);
    }
}

auto rhi::vk_query_shader_stage(shader shader) -> result<shader_stage>
{
    ZoneScoped;
    if (const auto* vk_shader = cast_from_handle<platform::vk_shader>(shader))
    {
        return platform::vk_rhi_translate_shader_stage(vk_shader->meta.stage);
    }

    return error("failed to access the shader");
}

auto rhi::vk_query_swapchain_images_count(swapchain swapchain) -> result<u32>
{
    ZoneScoped;
    if (const auto* vk_sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return vk_sc->sync_objects.size();
    }

    return error("failed to access the swapchain");
}

auto rhi::vk_query_current_frame_index(swapchain swapchain) -> result<u32>
{
    ZoneScoped;
    if (auto* vk_sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return vk_sc->frame_index;
    }

    return error("failed to access the swapchain");
}

auto rhi::vk_query_swapchain_color_format(swapchain swapchain) -> result<image_format>
{
    ZoneScoped;
    if (const auto* vk_sc = cast_from_handle<vk_rhi_swaphain_data>(swapchain))
    {
        return static_cast<image_format>(vk_sc->root.surface_format.format);
    }

    return error("failed to access the swapchain");
}

auto rhi::vk_query_feature_support(context context, const feature_flag feature) -> result<bool>
{
    ZoneScoped;
    if (const auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        return vk_ctx->enabled_device_features.supported(feature);
    }

    return error("failed to access the context");
}

auto rhi::vk_query_queue(context context, queue_kind kind) -> result<queue>
{
    ZoneScoped;
    if (auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        return queue {.id = reinterpret_cast<u64>(vk_ctx->queues[static_cast<u32>(kind)].queue)};
    }

    return error("failed to access the context");
}

auto rhi::vk_query_device(context context) -> result<device>
{
    ZoneScoped;
    if (auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        return device {.id = reinterpret_cast<u64>(vk_ctx->device)};
    }

    return error("failed to access the context");
}

auto rhi::vk_query_physical_device(context context) -> result<physical_device>
{
    ZoneScoped;
    if (auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        return physical_device {.id = reinterpret_cast<u64>(vk_ctx->physical_device)};
    }

    return error("failed to access the context");
}

void rhi::vk_queue_wait_idle(queue queue)
{
    ZoneScoped;
    if (const auto vk_queue = vk_rhi_vkobj_from_handle<VkQueue>(queue); vk_queue != VK_NULL_HANDLE)
    {
        vkQueueWaitIdle(vk_queue);
        return;
    }

    assert2m(false, "failed to access the queue");
}

void rhi::vk_device_wait_idle(context context)
{
    ZoneScoped;
    if (const auto* vk_ctx = cast_from_handle<platform::vk_context>(context))
    {
        vkDeviceWaitIdle(vk_ctx->device);
        return;
    }

    assert2m(false, "failed to access the device");
}

void rhi::vk_fence_wait_for_value(context context, fence fence, const u64 expected_value)
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_fence     = cast_from_handle<platform::vk_timeline_semaphore>(fence);

    assert2(vk_ctx && vk_fence);
    if (vk_ctx && vk_fence)
    {
        platform::vk_wait_on_timeline_semaphore(vk_ctx->device, *vk_fence, expected_value);
    }
}

auto rhi::vk_acquire_next_swapchain_image(context context, swapchain swapchain) -> result<image>
{
    ZoneScoped;
    const auto* vk_ctx = cast_from_handle<platform::vk_context>(context);
    auto* vk_sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);

    assert2(vk_ctx && vk_sc);
    if (!vk_ctx || !vk_sc)
    {
        return error("failed to access the context or the swapchain data");
    }

    vkWaitForFences(vk_ctx->device, 1, &vk_sc->sync_objects[vk_sc->frame_index].fence, VK_TRUE, UINT64_MAX);
    vkResetFences(vk_ctx->device, 1, &vk_sc->sync_objects[vk_sc->frame_index].fence);

    u32 new_image_index       = 0;
    const auto acquire_result = vkAcquireNextImageKHR(vk_ctx->device,
                                                      vk_sc->root.sc,
                                                      UINT64_MAX,
                                                      vk_sc->sync_objects[vk_sc->frame_index].acquire_semaphore,
                                                      VK_NULL_HANDLE,
                                                      &new_image_index);

    vk_sc->image_index = new_image_index & 0xFF;
    switch (acquire_result)
    {
    case VK_SUCCESS :
    case VK_SUBOPTIMAL_KHR :
        return image {.id = reinterpret_cast<u64>(&vk_sc->root.images[vk_sc->image_index].image)};
    case VK_ERROR_OUT_OF_DATE_KHR :
        return error("failed to acquire swapchain image (out of date)");
    default :
        return error("failed to acquire swapchain image (unknown result)");
    }
}

void rhi::vk_cmd_begin_recording(command_buffer cmd)
{
    ZoneScoped;
    if (auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd))
    {
        constexpr VkCommandBufferBeginInfo command_buffer_begin_info {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = 0};
        vkBeginCommandBuffer(vk_cmd->cmd_buffer, &command_buffer_begin_info);
        return;
    }

    assert2m(false, "failed to access the command buffer");
}

void rhi::vk_cmd_end_recording(command_buffer cmd)
{
    ZoneScoped;
    if (auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd))
    {
        vkEndCommandBuffer(vk_cmd->cmd_buffer);
        return;
    }

    assert2m(false, "failed to access the command buffer");
}

void rhi::vk_cmd_reset(command_buffer cmd)
{
    ZoneScoped;
    if (auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd))
    {
        vkResetCommandBuffer(vk_cmd->cmd_buffer, 0);
        return;
    }

    assert2m(false, "failed to access the command buffer");
}

void rhi::vk_cmd_barriers(command_buffer cmd, const barrier_batch& barriers)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    cpp::local_array<VkMemoryBarrier2> global_barriers(barriers.globals.size());
    cpp::local_array<VkImageMemoryBarrier2> image_barriers(barriers.images.size());
    cpp::local_array<VkBufferMemoryBarrier2> buffer_barriers(barriers.buffers.size());

    for (u32 i = 0; i < barriers.globals.size(); ++i)
    {
        global_barriers[i] = vk_rhi_parse_global_barrier(barriers.globals[i]);
    }

    for (u32 i = 0; i < barriers.images.size(); ++i)
    {
        image_barriers[i] = vk_rhi_parse_image_barrier(barriers.images[i]);
    }

    for (u32 i = 0; i < barriers.buffers.size(); ++i)
    {
        buffer_barriers[i] = vk_rhi_parse_buffer_barrier(barriers.buffers[i]);
    }

    const VkDependencyInfo dependency_info {
        .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                    = nullptr,
        .memoryBarrierCount       = static_cast<u32>(global_barriers.size()),
        .pMemoryBarriers          = global_barriers.data(),
        .bufferMemoryBarrierCount = static_cast<u32>(buffer_barriers.size()),
        .pBufferMemoryBarriers    = buffer_barriers.data(),
        .imageMemoryBarrierCount  = static_cast<u32>(image_barriers.size()),
        .pImageMemoryBarriers     = image_barriers.data(),
    };

    vkCmdPipelineBarrier2(vk_cmd->cmd_buffer, &dependency_info);
}

void rhi::vk_cmd_image_blit(command_buffer cmd, image src, image dst, const blit_image_info& info)
{
    ZoneScoped;
    const auto* vk_cmd       = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_src_image = rhi::cast_from_handle<platform::vk_image>(src);
    const auto* vk_dst_image = rhi::cast_from_handle<platform::vk_image>(dst);

    assert2(vk_cmd && vk_src_image && vk_dst_image);
    if (!vk_cmd || !vk_src_image || !vk_dst_image)
    {
        return;
    }

    VkImageBlit blit {
        .srcSubresource = {.aspectMask     = platform::vk_rhi_parse_aspect_flags(info.aspects),
                           .mipLevel       = info.src_mip,
                           .baseArrayLayer = info.src_layers.x,
                           .layerCount =
                               info.src_layers.y == rhi::kLayersAll ? VK_REMAINING_ARRAY_LAYERS : info.src_layers.y},
        .dstSubresource = {.aspectMask     = platform::vk_rhi_parse_aspect_flags(info.aspects),
                           .mipLevel       = info.dst_mip,
                           .baseArrayLayer = info.dst_layers.x,
                           .layerCount =
                               info.dst_layers.y == rhi::kLayersAll ? VK_REMAINING_ARRAY_LAYERS : info.dst_layers.y}
    };
    blit.srcOffsets[0] = {.x = info.src_offset.x, .y = info.src_offset.y, .z = info.src_offset.z};
    blit.srcOffsets[1] = {.x = info.src_extent.x, .y = info.src_extent.y, .z = info.src_extent.z};
    blit.dstOffsets[0] = {.x = info.dst_offset.x, .y = info.dst_offset.y, .z = info.dst_offset.z};
    blit.dstOffsets[1] = {.x = info.dst_extent.x, .y = info.dst_extent.y, .z = info.dst_extent.z};

    vkCmdBlitImage(vk_cmd->cmd_buffer,
                   vk_src_image->image,
                   vk_src_image->layout,
                   vk_dst_image->image,
                   vk_dst_image->layout,
                   1,
                   &blit,
                   platform::vk_rhi_parse_sampler_filter(info.filter));
}

void rhi::vk_cmd_copy_buffer(command_buffer cmd, buffer src_buffer, const u64vec2 src_range, buffer dst_buffer,
                             const u64 dst_offset)
{
    ZoneScoped;
    const auto* vk_cmd        = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_src_buffer = rhi::cast_from_handle<platform::vk_buffer>(src_buffer);
    const auto* vk_dst_buffer = rhi::cast_from_handle<platform::vk_buffer>(dst_buffer);

    assert2(vk_cmd && vk_src_buffer && vk_dst_buffer);
    if (!vk_cmd || !vk_src_buffer || !vk_dst_buffer)
    {
        return;
    }

    VkBufferCopy2 region {
        .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = src_range.x,
        .dstOffset = dst_offset,
        .size      = src_range.y,
    };

    const VkCopyBufferInfo2 copy_buffer_info {
        .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer   = vk_src_buffer->buffer,
        .dstBuffer   = vk_dst_buffer->buffer,
        .regionCount = 1,
        .pRegions    = &region,
    };

    vkCmdCopyBuffer2(vk_cmd->cmd_buffer, &copy_buffer_info);
}

void rhi::vk_cmd_clear_buffer(command_buffer cmd, buffer buffer, const u64vec2 range, const u32 value)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = rhi::cast_from_handle<platform::vk_buffer>(buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    vkCmdFillBuffer(vk_cmd->cmd_buffer,
                    vk_buffer->buffer,
                    range.x,
                    range.y == kBufferAll ? (vk_buffer->size - range.x) : range.y,
                    value);
}

void rhi::vk_cmd_update_buffer(command_buffer cmd, buffer buffer, const u64vec2 range, const void* data)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = rhi::cast_from_handle<platform::vk_buffer>(buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    vkCmdUpdateBuffer(
        vk_cmd->cmd_buffer, vk_buffer->buffer, range.x, range.y == kBufferAll ? VK_WHOLE_SIZE : range.y, data);
}

void rhi::vk_cmd_copy_buffer_to_image(command_buffer cmd, buffer src, image dst, image_layout layout,
                                      const std::span<const copy_image_info> regions)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = rhi::cast_from_handle<platform::vk_buffer>(src);
    const auto* vk_image  = rhi::cast_from_handle<platform::vk_image>(dst);

    assert2(vk_cmd && vk_buffer && vk_image);
    if (!vk_cmd || !vk_buffer || !vk_image)
    {
        return;
    }

    cpp::local_array<VkBufferImageCopy> vk_regions(regions.size());

    for (u32 i = 0; i < regions.size(); ++i)
    {
        auto& info    = regions[i];
        vk_regions[i] = {
            .bufferOffset      = info.source_offset,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {.aspectMask     = platform::vk_rhi_parse_aspect_flags(info.aspects),
                                  .mipLevel       = info.dst_mip,
                                  .baseArrayLayer = info.dst_layers.x,
                                  .layerCount     = info.dst_layers.y == rhi::kLayersAll ? VK_REMAINING_ARRAY_LAYERS
                                                                                         : info.dst_layers.y},
            .imageOffset       = {.x = info.dst_offset.x, .y = info.dst_offset.y, .z = info.dst_offset.z},
            .imageExtent       = {.width = info.dst_extent.x, .height = info.dst_extent.y, .depth = info.dst_extent.z},
        };
    }

    vkCmdCopyBufferToImage(vk_cmd->cmd_buffer,
                           vk_buffer->buffer,
                           vk_image->image,
                           platform::vk_rhi_parse_image_layout(layout),
                           vk_regions.size(),
                           vk_regions.data());
}

void rhi::vk_cmd_clear_depth_attachment(command_buffer cmd, image image, const ds_clear_value value)
{
    ZoneScoped;
    const auto* vk_cmd   = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_image = rhi::cast_from_handle<platform::vk_image>(image);

    assert2(vk_cmd && vk_image);
    if (!vk_cmd || !vk_image)
    {
        return;
    }

    const VkClearDepthStencilValue vk_value {.depth = value.depth, .stencil = value.stencil};
    const auto range = platform::vk_image_subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);

    vkCmdClearDepthStencilImage(vk_cmd->cmd_buffer, vk_image->image, vk_image->layout, &vk_value, 1, &range);
}

void rhi::vk_cmd_clear_color_attachment(command_buffer cmd, image image, const color_clear_value value)
{
    ZoneScoped;
    const auto* vk_cmd   = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_image = rhi::cast_from_handle<platform::vk_image>(image);

    assert2(vk_cmd && vk_image);
    if (!vk_cmd || !vk_image)
    {
        return;
    }

    const VkClearColorValue vk_color {
        .uint32 = {value.u4.x, value.u4.y, value.u4.z, value.u4.w}
    };

    const auto range = platform::vk_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(vk_cmd->cmd_buffer, vk_image->image, vk_image->layout, &vk_color, 1, &range);
}

void rhi::vk_cmd_set_draw_state(command_buffer cmd, std::span<const attachment_state_info> color_attachments,
                                attachment_state_info depth_attachment, uvec4 viewport)
{
    ZoneScoped;
    auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    VkRect2D vp = {static_cast<i32>(viewport.x), static_cast<i32>(viewport.y), viewport.z, viewport.w};
    VkRenderingInfo rendering_info {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, .renderArea = vp, .layerCount = 1};

    VkRenderingAttachmentInfo color_attachment_info[16] {};
    assert2(color_attachments.size() <= COUNT_OF(color_attachment_info));
    for (u32 i = 0; i < cpp::min(color_attachments.size(), COUNT_OF(color_attachment_info)); i++)
    {
        auto info = vk_rhi_parse_attachment_info(color_attachments[i], false);

        assert2(info);
        if (!info)
        {
            return;
        }

        color_attachment_info[i] = *info;
    }

    if (!color_attachments.empty())
    {
        rendering_info.pColorAttachments    = color_attachment_info;
        rendering_info.colorAttachmentCount = color_attachments.size();
    }

    VkRenderingAttachmentInfo depth_attachment_info {};
    if (depth_attachment)
    {
        auto info = vk_rhi_parse_attachment_info(depth_attachment, true);

        assert2(info);
        if (!info)
        {
            return;
        }

        depth_attachment_info           = *info;
        rendering_info.pDepthAttachment = &depth_attachment_info;
    }

    vkCmdBeginRendering(vk_cmd->cmd_buffer, &rendering_info);

    const VkViewport vkvp {.x        = static_cast<f32>(vp.offset.x),
                           .y        = static_cast<f32>(vp.offset.y),
                           .width    = static_cast<f32>(vp.extent.width),
                           .height   = static_cast<f32>(vp.extent.height),
                           .minDepth = 0.0F,
                           .maxDepth = 1.0F};

    vkCmdSetCullMode(vk_cmd->cmd_buffer, VK_CULL_MODE_NONE);
    vkCmdSetScissor(vk_cmd->cmd_buffer, 0, 1, &vp);
    vkCmdSetViewport(vk_cmd->cmd_buffer, 0, 1, &vkvp);
}

void rhi::vk_cmd_clear_draw_state(command_buffer cmd)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    vkCmdEndRendering(vk_cmd->cmd_buffer);
}

void rhi::vk_cmd_set_cull_mode(command_buffer cmd, cull_mode mode)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    VkCullModeFlags vk_cull_mode = VK_CULL_MODE_NONE;
    switch (mode)
    {
    case cull_mode::back :
        vk_cull_mode = VK_CULL_MODE_BACK_BIT;
        break;
    case cull_mode::front :
        vk_cull_mode = VK_CULL_MODE_FRONT_BIT;
        break;
    case cull_mode::all :
        vk_cull_mode = VK_CULL_MODE_FRONT_AND_BACK;
        break;
    default :
        break;
    }

    vkCmdSetCullMode(vk_cmd->cmd_buffer, vk_cull_mode);
}

void rhi::vk_cmd_set_depth_bias(command_buffer cmd, const f32 constant_factor, const f32 slope_factor, const f32 clamp)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    vkCmdSetDepthBias(vk_cmd->cmd_buffer, constant_factor, clamp, slope_factor);
}

void rhi::vk_cmd_bind_pso(command_buffer cmd, pipeline pso)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_pso = cast_from_handle<platform::vk_pipeline>(pso);

    assert2(vk_cmd && vk_pso);
    if (!vk_cmd || !vk_pso)
    {
        return;
    }

    vk_pso->bind(vk_cmd->cmd_buffer);
}

void rhi::vk_cmd_bind_index(command_buffer cmd, buffer index_buffer)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = cast_from_handle<platform::vk_buffer>(index_buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    vkCmdBindIndexBuffer(vk_cmd->cmd_buffer, vk_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void rhi::vk_cmd_push_constants(command_buffer cmd, pipeline pso, const void* data, const u32 size, const u32 offset)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_pso = cast_from_handle<platform::vk_pipeline>(pso);

    assert2(vk_cmd && vk_pso);
    if (!vk_cmd || !vk_pso)
    {
        return;
    }

    vk_pso->push_constant(vk_cmd->cmd_buffer, offset, size, data);
}

void rhi::vk_cmd_push_bindings(command_buffer cmd, pipeline pso, const std::span<const binding> bindings)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_pso = cast_from_handle<platform::vk_pipeline>(pso);

    assert2(vk_cmd && vk_pso);
    if (!vk_cmd || !vk_pso)
    {
        return;
    }

    platform::vk_descriptor_bindings vk_bindings;
    assert2(bindings.size() < platform::vk_descriptor_bindings::kMaxSetZeroBindings);

    const u32 bindings_count = cpp::min<u32>(bindings.size(), platform::vk_descriptor_bindings::kMaxSetZeroBindings);
    for (u32 i = 0; i < bindings_count; i++)
    {
        vk_bindings.bind_at(vk_rhi_parse_descriptor_info(bindings[i]), i);
    }

    vk_pso->push_descriptor_set(vk_cmd->cmd_buffer, vk_bindings.get());
}

void rhi::vk_cmd_push_bindless_set(command_buffer cmd, pipeline pso, bindless_set set, const u32 binding)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_pso = cast_from_handle<platform::vk_pipeline>(pso);
    const auto* vk_set = cast_from_handle<platform::vk_descriptor_set>(set);

    assert2(vk_cmd && vk_pso && vk_set);
    if (!vk_cmd || !vk_pso || !vk_set)
    {
        return;
    }

    vk_pso->bind_descriptor_set(vk_cmd->cmd_buffer, *vk_set, binding);
}

void rhi::vk_cmd_dispatch(command_buffer cmd, pipeline pso, const uvec3 threads)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_pso = cast_from_handle<platform::vk_pipeline>(pso);

    if (vk_cmd && vk_pso)
    {
        vk_pso->dispatch(vk_cmd->cmd_buffer, threads.x, threads.y, threads.z);
    }
}

void rhi::vk_cmd_dispatch_indirect(command_buffer cmd, buffer count_buffer, const u32 buffer_offset)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = cast_from_handle<platform::vk_buffer>(count_buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    assert2(buffer_offset < vk_buffer->size);
    vkCmdDispatchIndirect(vk_cmd->cmd_buffer, vk_buffer->buffer, buffer_offset);
}

void rhi::vk_cmd_draw(command_buffer cmd, const u32 vtx_count, const u32 instance_count, const u32 first_vertex,
                      const u32 first_instance)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);

    assert2(vk_cmd);
    if (!vk_cmd)
    {
        return;
    }

    vkCmdDraw(vk_cmd->cmd_buffer, vtx_count, instance_count, first_vertex, first_instance);
}

void rhi::vk_cmd_draw_indexed_indirect(command_buffer cmd, buffer buffer, const u64 offset, const u32 count,
                                       const u32 stride)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = cast_from_handle<platform::vk_buffer>(buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    assert2(offset < vk_buffer->size);
    vkCmdDrawIndexedIndirect(vk_cmd->cmd_buffer, vk_buffer->buffer, offset, count, stride);
}

void rhi::vk_cmd_draw_indexed_indirect_count(command_buffer cmd, rhi::buffer buffer, const u64 offset,
                                             rhi::buffer count_buffer, const u64 count_offset, const u32 max_count,
                                             const u32 stride)
{
    ZoneScoped;
    const auto* vk_cmd          = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer       = cast_from_handle<platform::vk_buffer>(buffer);
    const auto* vk_buffer_count = cast_from_handle<platform::vk_buffer>(count_buffer);

    assert2(vk_cmd && vk_buffer && vk_buffer_count);
    if (!vk_cmd || !vk_buffer || !vk_buffer_count)
    {
        return;
    }

    assert2(offset < vk_buffer->size);
    vkCmdDrawIndexedIndirectCount(
        vk_cmd->cmd_buffer, vk_buffer->buffer, offset, vk_buffer_count->buffer, count_offset, max_count, stride);
}

void rhi::vk_cmd_draw_mesh_indirect(command_buffer cmd, buffer buffer, const u64 offset, const u32 count,
                                    const u32 stride)
{
    ZoneScoped;
    const auto* vk_cmd    = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer = cast_from_handle<platform::vk_buffer>(buffer);

    assert2(vk_cmd && vk_buffer);
    if (!vk_cmd || !vk_buffer)
    {
        return;
    }

    assert2(offset < vk_buffer->size);
    vkCmdDrawMeshTasksIndirectEXT(vk_cmd->cmd_buffer, vk_buffer->buffer, offset, count, stride);
}

void rhi::vk_cmd_draw_mesh_indirect_count(command_buffer cmd, rhi::buffer buffer, const u64 offset,
                                          rhi::buffer count_buffer, const u64 count_offset, const u32 max_count,
                                          const u32 stride)
{
    ZoneScoped;
    const auto* vk_cmd          = cast_from_handle<platform::vk_command_buffer>(cmd);
    const auto* vk_buffer       = cast_from_handle<platform::vk_buffer>(buffer);
    const auto* vk_buffer_count = cast_from_handle<platform::vk_buffer>(count_buffer);
    assert2(vk_cmd && vk_buffer && vk_buffer_count);
    if (!vk_cmd || !vk_buffer || !vk_buffer_count)
    {
        return;
    }

    assert2(offset < vk_buffer->size);
    vkCmdDrawMeshTasksIndirectCountEXT(
        vk_cmd->cmd_buffer, vk_buffer->buffer, offset, vk_buffer_count->buffer, count_offset, max_count, stride);
}

void rhi::vk_submit(context context, queue queue, const submit_info& info)
{
    ZoneScoped;
    const auto* vk_ctx  = cast_from_handle<platform::vk_context>(context);
    const auto vk_queue = vk_rhi_vkobj_from_handle<VkQueue>(queue);

    assert2(vk_ctx && vk_queue != VK_NULL_HANDLE);
    if (!vk_ctx || vk_queue == VK_NULL_HANDLE)
    {
        return;
    }

    cpp::local_array<VkSemaphoreSubmitInfo> wait_semaphore_submit_info(info.waits.size());
    cpp::local_array<VkSemaphoreSubmitInfo> signal_semaphore_submit_info(info.signals.size());
    cpp::local_array<VkCommandBufferSubmitInfo> command_buffers_submit_info(info.command_buffers.size());

    for (u32 i = 0; i < info.waits.size(); ++i)
    {
        auto* vk_fence = cast_from_handle<platform::vk_timeline_semaphore>(info.waits[i].fence);

        assert2(vk_fence);
        if (!vk_fence)
        {
            return;
        }

        wait_semaphore_submit_info[i] = {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk_fence->semaphore,
            .value     = info.waits[i].value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        };

        vk_fence->last_value = info.waits[i].value;
    }

    for (u32 i = 0; i < info.signals.size(); ++i)
    {
        auto* vk_fence = cast_from_handle<platform::vk_timeline_semaphore>(info.signals[i].fence);

        assert2(vk_fence);
        if (!vk_fence)
        {
            return;
        }

        signal_semaphore_submit_info[i] = {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk_fence->semaphore,
            .value     = info.signals[i].value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        };

        vk_fence->last_value = info.signals[i].value;
    }

    for (u32 i = 0; i < info.command_buffers.size(); ++i)
    {
        const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(info.command_buffers[i]);

        assert2(vk_cmd);
        if (!vk_cmd)
        {
            return;
        }

        command_buffers_submit_info[i] = {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = vk_cmd->cmd_buffer,
        };
    }

    const VkSubmitInfo2 submit_info {
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = static_cast<u32>(wait_semaphore_submit_info.size()),
        .pWaitSemaphoreInfos      = wait_semaphore_submit_info.data(),
        .commandBufferInfoCount   = static_cast<u32>(command_buffers_submit_info.size()),
        .pCommandBufferInfos      = command_buffers_submit_info.data(),
        .signalSemaphoreInfoCount = static_cast<u32>(signal_semaphore_submit_info.size()),
        .pSignalSemaphoreInfos    = signal_semaphore_submit_info.data(),
    };

    vkQueueSubmit2(vk_queue, 1, &submit_info, VK_NULL_HANDLE);
}

void rhi::vk_present(command_buffer cmd, swapchain swapchain, queue submit, queue present)
{
    ZoneScoped;
    const auto* vk_cmd = cast_from_handle<platform::vk_command_buffer>(cmd);
    auto* vk_sc        = cast_from_handle<vk_rhi_swaphain_data>(swapchain);
    if (!vk_cmd || !vk_sc)
    {
        return;
    }

    const VkSemaphoreSubmitInfo wait_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = vk_sc->sync_objects[vk_sc->frame_index].acquire_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    const VkSemaphoreSubmitInfo signal_semaphore_info {
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext     = nullptr,
        .semaphore = vk_sc->root.images[vk_sc->image_index].release_semaphore,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    const VkCommandBufferSubmitInfo command_buffer_submit_info {
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext         = nullptr,
        .commandBuffer = vk_cmd->cmd_buffer,
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
        vk_rhi_vkobj_from_handle<VkQueue>(submit), 1, &gfx_submit_info, vk_sc->sync_objects[vk_sc->frame_index].fence));

    const VkPresentInfoKHR present_info_khr {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vk_sc->root.images[vk_sc->image_index].release_semaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &vk_sc->root.sc,
        .pImageIndices      = &vk_sc->image_index,
        .pResults           = nullptr,
    };

    // ReSharper disable once CppTooWideScope
    const auto present_result = vkQueuePresentKHR(vk_rhi_vkobj_from_handle<VkQueue>(present), &present_info_khr);
    switch (present_result)
    {
    case VK_SUBOPTIMAL_KHR :
    case VK_ERROR_OUT_OF_DATE_KHR :
        return;
    default :
        VK_ASSERT_ON_FAIL(present_result);
    }

    vk_sc->frame_index = (vk_sc->frame_index + 1) % vk_sc->sync_objects.size();
}
