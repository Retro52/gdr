#pragma once

#include <app/pso.hpp>
#include <imgui.h>
#include <render/platform/vk/vk_pipeline.hpp>
#include <render/platform/vk/vk_renderer.hpp>
#include <window.hpp>

#include <vector>

class imgui_layer
{
public:
    imgui_layer(const window& window, const render::vk_renderer& renderer, app::pso_data& pipelines);

    ~imgui_layer();

    void begin_frame();

    void end_frame(const render::vk_renderer& renderer);

    void image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv = {0, 0, 1, 1},
               ImVec2 size = {256, 256}, f32 brightness = 0.0F, f32 mip = 0.0F);

    void image_array(VkImage image, VkImageView view, VkImageLayout src_layout, f32 layer, vec4 uv = {0, 0, 1, 1},
                     ImVec2 size = {256, 256}, f32 brightness = 0.0F, f32 mip = 0.0F);

    void depth_image(VkImage image, VkImageView view, VkImageLayout src_layout, vec4 uv = {0, 0, 1, 1},
                     ImVec2 size = {256, 256}, f32 brightness = 1.0F, f32 mip = 0.0F);

private:
    enum class sampler_type : u8
    {
        sampler2d,
        sampler2d_array,
    };

    struct pc_data
    {
        f32 mip_level;
        f32 brightness;
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
        render::vk_image atlas_image;
        VkSampler sampler {VK_NULL_HANDLE};
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

    void flush_pending(VkCommandBuffer cmd);

    bool allocate_region(u32 w, u32 h, VkOffset2D& out_offset);

    void image_impl(VkImage image, VkImageView view, ImVec2 size, ImVec2 uv0, ImVec2 uv1, VkImageLayout src_layout,
                    VkImageAspectFlags aspect, sampler_type type, const pc_data& push_constant);

private:
    constexpr static u32 kAtlasWidth   = 4096;
    constexpr static u32 kAtlasHeight  = 4096;
    constexpr static u32 kAtlasPadding = 4;

    // TODO: keep an array of atlases?
    atlas_data m_atlas_data;
    std::vector<blit_request> m_pending_uploads;

    app::pso_data& m_pipelines;
    const render::vk_renderer& m_renderer;
};
