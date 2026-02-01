#pragma once

#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <window.hpp>

class imgui_layer
{
public:
    imgui_layer(const window& window, const render::vk_renderer& renderer);

    ~imgui_layer();

    void begin_frame();

    void end_frame(VkCommandBuffer cmd);

    void flush_pending(VkCommandBuffer cmd);

    void image(VkImage image, VkImageView view, VkImageLayout src_layout, ImVec2 size = {256, 256});
    void depth_image(VkImage image, VkImageView view, VkImageLayout src_layout, ImVec2 size = {256, 256});

private:
    struct blit_request
    {
        VkImage img;
        VkImageView view;
        VkOffset2D offset;
        VkExtent2D extent;
        VkImageLayout src_layout;
        VkImageAspectFlags aspect;
    };

    struct atlas_data
    {
        render::vk_image atlas_image;
        VkSampler sampler {VK_NULL_HANDLE};
        VkImageView atlas_view {VK_NULL_HANDLE};
        VkDescriptorSet imgui_descriptor {VK_NULL_HANDLE};

        u32 cursor_x {0};
        u32 cursor_y {0};
        u32 cursor_row {0};

        void reset_cursor()
        {
            cursor_x   = 0;
            cursor_y   = 0;
            cursor_row = 0;
        }
    };

    bool allocate_region(u32 w, u32 h, VkOffset2D& out_offset);

    void image_impl(VkImage image, VkImageView view, ImVec2 size, VkImageLayout src_layout, VkImageAspectFlags aspect);

private:
    constexpr static u32 kAtlasWidth {4096};
    constexpr static u32 kAtlasHeight {4096};

    // TODO: keep an array of atlases?
    atlas_data m_atlas_data;
    render::vk_pipeline m_blit_pipeline;
    std::vector<blit_request> m_pending_uploads;

    const render::vk_renderer& m_renderer;
};
