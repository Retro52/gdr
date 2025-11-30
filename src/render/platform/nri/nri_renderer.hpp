#pragma once

#include <types.hpp>

#include <NRI.h>
#include <render/platform/nri/nri_interface.hpp>
#include <window.hpp>

#include <Extensions/NRIStreamer.h>
#include <Extensions/NRISwapChain.h>

class nri_renderer
{
private:
    struct queued_frame
    {
        nri::CommandBuffer* command_buffer;
        nri::CommandAllocator* command_allocator;
    };

    struct swap_chain_texture
    {
        nri::Texture* texture;
        nri::Fence* acquire_semaphore;
        nri::Fence* release_semaphore;
        nri::Format attachment_format;
        nri::Descriptor* color_attachment;
    };

    struct renderer_desc
    {
        u32 frames_in_flight {2};
        bool present_vsync {false};
    };

public:
    nri_renderer(const window& window, const renderer_desc& desc = {});

    void begin_frame();

    void present();

    void latency_sleep();

    void resize_swap_chain(const window& window, ivec2 new_size);

    nri_interface& get_nri();

    nri::Device* get_device();

    nri::SwapChain* get_swap_chain();

    nri::Texture* get_swap_chain_texture();

    nri::Descriptor* get_color_attachment();

    nri::Format get_swap_chain_attachment_format();

    nri::CommandBuffer* begin_command_buffer();

    void end_command_buffer(nri::CommandBuffer* command_buffer);

    void barrier_color_texture_to_render(nri::CommandBuffer* command_buffer, nri::Texture* texture);

    void barrier_color_texture_to_present(nri::CommandBuffer* command_buffer, nri::Texture* texture);

private:
    void create_swap_chain(const nri::Window& window, ivec2 new_size, u32 frames_in_flight, bool vsync);

private:
    std::vector<queued_frame> m_queued_frames;
    std::vector<swap_chain_texture> m_swap_chain_textures;

    nri_interface m_nri {};

    nri::Device* m_device {nullptr};
    nri::Queue* m_gfx_queue {nullptr};
    nri::Fence* m_frame_fence {nullptr};
    nri::Streamer* m_streamer {nullptr};
    nri::SwapChain* m_swap_chain {nullptr};
    nri::DescriptorPool* m_descriptor_pool {nullptr};

    nri::Fence* m_swap_chain_acquire_semaphore {nullptr};

    nri::Format m_swap_chain_attachment_format {};

    u32 m_swap_chain_index {0};

    u32 m_frame_index {0};
    u32 m_frame_count {0};

    bool m_vsync {false};
};
