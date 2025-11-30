#if 0

#include <aligned_alloc.hpp>
#include <render/platform/nri/nri_interface.hpp>
#include <render/platform/nri/nri_renderer.hpp>

#include <Extensions/NRIDeviceCreation.h>
#include <Extensions/NRISwapChain.h>

#define NRI_ABORT_ON_FAILURE(result)      \
    if ((result) != nri::Result::SUCCESS) \
    {                                     \
        exit(1);                          \
    }

#define NRI_LOAD_INTERFACE(DEVICE, INTERFACE, NRI) \
    NRI_ABORT_ON_FAILURE(nri::nriGetInterface(DEVICE, NRI_INTERFACE(INTERFACE), (INTERFACE*)&(NRI)))

constexpr nri::VKBindingOffsets VK_BINDING_OFFSETS = {0, 128, 32, 64};  // see CMake

void NRI_CALL nri_free(void*, void* memory)
{
    free_aligned(memory);
}

void* NRI_CALL nri_alloc(void*, size_t size, size_t alignment)
{
    return alloc_aligned(size, alignment);
}

void* NRI_CALL nri_realloc(void*, void* memory, size_t size, size_t alignment)
{
    return realloc_aligned(memory, size, alignment);
}

nri_renderer::nri_renderer(const window& window, const renderer_desc& desc)
    : m_vsync(desc.present_vsync)
{
    nri::AllocationCallbacks callbacks {
        .Allocate                           = nri_alloc,
        .Reallocate                         = nri_realloc,
        .Free                               = nri_free,
        .userArg                            = nullptr,
        .disable3rdPartyAllocationCallbacks = false,
    };

    u32 adapters_count                = 2;
    nri::AdapterDesc adapters_desc[2] = {};
    NRI_ABORT_ON_FAILURE(nri::nriEnumerateAdapters(adapters_desc, adapters_count));

    u32 device_index = (adapters_count > 0 && adapters_desc[1].architecture == nri::Architecture::DESCRETE) ? 1 : 0;

    nri::DeviceCreationDesc device_create_desc           = {};
    device_create_desc.graphicsAPI                       = nri::GraphicsAPI::VK;
    device_create_desc.enableNRIValidation               = true;
    device_create_desc.enableGraphicsAPIValidation       = true;
    device_create_desc.enableD3D11CommandBufferEmulation = false;
    device_create_desc.disableD3D12EnhancedBarriers      = false;
    device_create_desc.vkBindingOffsets                  = VK_BINDING_OFFSETS;
    device_create_desc.adapterDesc                       = &adapters_desc[device_index];
    device_create_desc.allocationCallbacks               = callbacks;

    NRI_ABORT_ON_FAILURE(nri::nriCreateDevice(device_create_desc, m_device));

    // NRI
    NRI_LOAD_INTERFACE(*m_device, nri::CoreInterface, m_nri);
    NRI_LOAD_INTERFACE(*m_device, nri::ImguiInterface, m_nri);
    NRI_LOAD_INTERFACE(*m_device, nri::HelperInterface, m_nri);
    NRI_LOAD_INTERFACE(*m_device, nri::StreamerInterface, m_nri);
    NRI_LOAD_INTERFACE(*m_device, nri::SwapChainInterface, m_nri);

    nri::StreamerDesc streamer_desc           = {};
    streamer_desc.dynamicBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
    streamer_desc.dynamicBufferDesc = {0, 0, nri::BufferUsageBits::VERTEX_BUFFER | nri::BufferUsageBits::INDEX_BUFFER};
    streamer_desc.constantBufferMemoryLocation = nri::MemoryLocation::HOST_UPLOAD;
    streamer_desc.queuedFrameNum               = desc.frames_in_flight;

    NRI_ABORT_ON_FAILURE(m_nri.CreateFence(*m_device, 0, m_frame_fence));
    NRI_ABORT_ON_FAILURE(m_nri.CreateStreamer(*m_device, streamer_desc, m_streamer));
    NRI_ABORT_ON_FAILURE(m_nri.GetQueue(*m_device, nri::QueueType::GRAPHICS, 0, m_gfx_queue));

    create_swap_chain(window.get_nri_window(), window.get_size_in_px(), desc.frames_in_flight, m_vsync);

    m_queued_frames.resize(desc.frames_in_flight);
    for (queued_frame& queued_frame : m_queued_frames)
    {
        NRI_ABORT_ON_FAILURE(m_nri.CreateCommandAllocator(*m_gfx_queue, queued_frame.command_allocator));
        NRI_ABORT_ON_FAILURE(m_nri.CreateCommandBuffer(*queued_frame.command_allocator, queued_frame.command_buffer));
    }

    {
        nri::DescriptorPoolDesc descriptor_pool_desc = {};
        descriptor_pool_desc.descriptorSetMaxNum     = m_queued_frames.size() + 1;
        descriptor_pool_desc.constantBufferMaxNum    = m_queued_frames.size();
        descriptor_pool_desc.textureMaxNum           = 1;

        NRI_ABORT_ON_FAILURE(m_nri.CreateDescriptorPool(*m_device, descriptor_pool_desc, m_descriptor_pool));
    }
}

void nri_renderer::begin_frame()
{
    m_swap_chain_acquire_semaphore = m_swap_chain_textures[m_frame_index].acquire_semaphore;
    m_nri.AcquireNextTexture(*m_swap_chain, *m_swap_chain_acquire_semaphore, m_swap_chain_index);
}

void nri_renderer::present()
{
    const queued_frame& frame = m_queued_frames[m_frame_index];
    auto& current_sc_texture  = m_swap_chain_textures[m_swap_chain_index];

    {  // Submit
        nri::FenceSubmitDesc texture_acquired_fence = {};
        texture_acquired_fence.fence                = m_swap_chain_acquire_semaphore;
        texture_acquired_fence.stages               = nri::StageBits::COLOR_ATTACHMENT;

        nri::FenceSubmitDesc rendering_finished_fence = {};
        rendering_finished_fence.fence                = current_sc_texture.release_semaphore;

        nri::QueueSubmitDesc queue_submit_desc = {};
        queue_submit_desc.waitFences           = &texture_acquired_fence;
        queue_submit_desc.waitFenceNum         = 1;
        queue_submit_desc.commandBuffers       = &frame.command_buffer;
        queue_submit_desc.commandBufferNum     = 1;
        queue_submit_desc.signalFences         = &rendering_finished_fence;
        queue_submit_desc.signalFenceNum       = 1;

        m_nri.QueueSubmit(*m_gfx_queue, queue_submit_desc);
    }

    m_nri.EndStreamerFrame(*m_streamer);

    // Present
    nri::Result result = m_nri.QueuePresent(*m_swap_chain, *current_sc_texture.release_semaphore);

    // TODO: Handle
    if (result == nri::Result::DEVICE_LOST)
    {
    }

    {  // Signaling after "Present" improves D3D11 performance a bit
        nri::FenceSubmitDesc signal_fence = {};
        signal_fence.fence                = m_frame_fence;
        signal_fence.value                = 1 + m_frame_count;

        nri::QueueSubmitDesc queue_submit_desc = {};
        queue_submit_desc.signalFences         = &signal_fence;
        queue_submit_desc.signalFenceNum       = 1;

        m_nri.QueueSubmit(*m_gfx_queue, queue_submit_desc);
    }

    ++m_frame_count;
    m_frame_index = (m_frame_index + 1) % m_queued_frames.size();
}

void nri_renderer::latency_sleep()
{
    m_nri.Wait(*m_frame_fence,
               m_frame_count >= m_queued_frames.size() ? 1 + m_frame_count - m_queued_frames.size() : 0);
    m_nri.ResetCommandAllocator(*m_queued_frames[m_frame_index].command_allocator);
}

void nri_renderer::resize_swap_chain(const window& window, ivec2 new_size)
{
    create_swap_chain(window.get_nri_window(), new_size, m_queued_frames.size(), m_vsync);
}

nri_interface& nri_renderer::get_nri()
{
    return m_nri;
}

nri::Device* nri_renderer::get_device()
{
    return m_device;
}

nri::SwapChain* nri_renderer::get_swap_chain()
{
    return m_swap_chain;
}

nri::Texture* nri_renderer::get_swap_chain_texture()
{
    return m_swap_chain_textures[m_swap_chain_index].texture;
}

nri::Descriptor* nri_renderer::get_color_attachment()
{
    return m_swap_chain_textures[m_swap_chain_index].color_attachment;
}

nri::Format nri_renderer::get_swap_chain_attachment_format()
{
    return m_swap_chain_attachment_format;
}

nri::CommandBuffer* nri_renderer::begin_command_buffer()
{
    auto* cmd_buffer = m_queued_frames[m_frame_index].command_buffer;
    m_nri.BeginCommandBuffer(*cmd_buffer, m_descriptor_pool);

    return cmd_buffer;
}

void nri_renderer::end_command_buffer(nri::CommandBuffer* command_buffer)
{
    m_nri.EndCommandBuffer(*command_buffer);
}

void nri_renderer::barrier_color_texture_to_render(nri::CommandBuffer* command_buffer, nri::Texture* texture)
{
    nri::TextureBarrierDesc to_color = {};
    to_color.texture                 = texture;
    to_color.after                   = {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT};

    nri::BarrierDesc b0 = {};
    b0.textureNum       = 1;
    b0.textures         = &to_color;
    m_nri.CmdBarrier(*command_buffer, b0);
}

void nri_renderer::barrier_color_texture_to_present(nri::CommandBuffer* command_buffer, nri::Texture* texture)
{
    nri::TextureBarrierDesc to_present = {};
    to_present.texture                 = texture;
    to_present.before                  = {nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT};
    to_present.after                   = {nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE};

    nri::BarrierDesc b1 = {};
    b1.textureNum       = 1;
    b1.textures         = &to_present;
    m_nri.CmdBarrier(*command_buffer, b1);
}

void nri_renderer::create_swap_chain(const nri::Window& window, ivec2 new_size, u32 frames_in_flight, bool vsync)
{
    // Wait for idle
    if (m_gfx_queue)
    {
        m_nri.QueueWaitIdle(m_gfx_queue);
    }

    // Destroy old swap chain
    for (auto& sc_texture : m_swap_chain_textures)
    {
        m_nri.DestroyFence(sc_texture.acquire_semaphore);
        m_nri.DestroyFence(sc_texture.release_semaphore);
        m_nri.DestroyDescriptor(sc_texture.color_attachment);
    }

    if (m_swap_chain)
    {
        m_nri.DestroySwapChain(m_swap_chain);
    }

    nri::SwapChainDesc swap_chain_desc = {};
    swap_chain_desc.window             = window;
    swap_chain_desc.queue              = m_gfx_queue;
    swap_chain_desc.format             = nri::SwapChainFormat::BT709_G22_8BIT;
    swap_chain_desc.flags =
        (vsync ? nri::SwapChainBits::VSYNC : nri::SwapChainBits::NONE) | nri::SwapChainBits::ALLOW_TEARING;
    swap_chain_desc.width          = static_cast<u16>(new_size.x);
    swap_chain_desc.height         = static_cast<u16>(new_size.y);
    swap_chain_desc.textureNum     = frames_in_flight + 1;
    swap_chain_desc.queuedFrameNum = frames_in_flight;

    NRI_ABORT_ON_FAILURE(m_nri.CreateSwapChain(*m_device, swap_chain_desc, m_swap_chain));

    u32 swap_chain_tex_count;
    nri::Texture* const* swap_chain_textures = m_nri.GetSwapChainTextures(*m_swap_chain, swap_chain_tex_count);

    m_swap_chain_attachment_format = m_nri.GetTextureDesc(*swap_chain_textures[0]).format;

    m_swap_chain_textures.resize(swap_chain_tex_count);
    for (u32 i = 0; i < swap_chain_tex_count; i++)
    {
        nri::Texture2DViewDesc tex_view_descriptor = {
            swap_chain_textures[i], nri::Texture2DViewType::COLOR_ATTACHMENT, m_swap_chain_attachment_format};

        nri::Descriptor* color_attachment = nullptr;
        NRI_ABORT_ON_FAILURE(m_nri.CreateTexture2DView(tex_view_descriptor, color_attachment));

        nri::Fence* acquire_semaphore = nullptr;
        NRI_ABORT_ON_FAILURE(m_nri.CreateFence(*m_device, nri::SWAPCHAIN_SEMAPHORE, acquire_semaphore));

        nri::Fence* release_semaphore = nullptr;
        NRI_ABORT_ON_FAILURE(m_nri.CreateFence(*m_device, nri::SWAPCHAIN_SEMAPHORE, release_semaphore));

        m_swap_chain_textures[i] = {
            .texture           = swap_chain_textures[i],
            .acquire_semaphore = acquire_semaphore,
            .release_semaphore = release_semaphore,
            .attachment_format = m_swap_chain_attachment_format,
            .color_attachment  = color_attachment,
        };
    }
}

#endif
