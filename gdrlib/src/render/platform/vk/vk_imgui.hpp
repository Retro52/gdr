#pragma once

#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_image.hpp>
#include <render/platform/vk/vk_pipeline.hpp>

#include <imgui.h>
#include <vector>

namespace platform
{
    struct vk_imgui
    {
        constexpr static u32 kAtlasWidth   = 4096;
        constexpr static u32 kAtlasHeight  = 4096;
        constexpr static u32 kAtlasPadding = 4;

        enum class sampler_type : u8
        {
            sampler2d,
            sampler2d_array,
        };

        struct pc_data
        {
            f32 znear;
            f32 mip_level;
            f32 array_layer;
        };

        struct blit_request
        {
            VkImage img;
            VkImageView view;
            VkOffset2D offset;
            VkExtent2D extent;
            VkImageLayout src_layout;
            VkImageAspectFlags aspect;

            sampler_type type;
            pc_data push_constant;
        };

        struct atlas_data
        {
            platform::vk_image atlas_image;

            VkSampler sampler                = VK_NULL_HANDLE;
            VkDescriptorSet imgui_descriptor = VK_NULL_HANDLE;

            uvec3 cursor;

            void reset_cursor() { cursor = uvec3(0); }
        };

        platform::vk_pipeline blit2d;
        platform::vk_pipeline blit2d_array;

        atlas_data atlas_data;
        std::vector<blit_request> pending_uploads;

        void begin_frame();

        void end_frame(VkImageView rtv, VkCommandBuffer cmd, uvec4 vp);

        void image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv = {0, 0, 1, 1},
                   ImVec2 size = {256, 256}, f32 mip = 0.0F, f32 znear = 0.0F);

        void image_array(VkImage image, VkImageView view, VkImageLayout src_layout, f32 layer, vec4 uv = {0, 0, 1, 1},
                         ImVec2 size = {256, 256}, f32 mip = 0.0F, f32 znear = 0.0F);

        void depth_image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv = {0, 0, 1, 1},
                         ImVec2 size = {256, 256}, f32 mip = 0.0F, f32 znear = 0.0F);

        void flush_pending(VkCommandBuffer cmd);

        bool allocate_region(u32 w, u32 h, VkOffset2D& out_offset);

        void image_impl(VkImage image, VkImageView view, ImVec2 size, ImVec2 uv0, ImVec2 uv1, VkImageLayout src_layout,
                        VkImageAspectFlags aspect, sampler_type type, const pc_data& push_constant);
    };

    result<vk_imgui> vk_imgui_create_context(const platform::vk_context& ctx, SDL_Window* window, VkFormat format,
                                             u32 sc_image_count);

    void vk_imgui_destroy_context(const platform::vk_context& ctx, vk_imgui& context);
}
