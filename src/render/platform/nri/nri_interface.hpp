#pragma once

#include <NRI.h>

#include <Extensions/NRIHelper.h>
#include <Extensions/NRIImgui.h>
#include <Extensions/NRILowLatency.h>
#include <Extensions/NRIMeshShader.h>
#include <Extensions/NRIRayTracing.h>
#include <Extensions/NRIStreamer.h>
#include <Extensions/NRISwapChain.h>
#include <Extensions/NRIUpscaler.h>

class nri_interface
    : public nri::CoreInterface
    , public nri::HelperInterface
    , public nri::LowLatencyInterface
    , public nri::MeshShaderInterface
    , public nri::RayTracingInterface
    , public nri::StreamerInterface
    , public nri::SwapChainInterface
    , public nri::UpscalerInterface
    , public nri::ImguiInterface
{
public:
    [[nodiscard]] bool has_core() const
    {
        return GetDeviceDesc != nullptr;
    }

    [[nodiscard]] bool has_ray_tracing() const
    {
        return CreateRayTracingPipeline != nullptr;
    }

    [[nodiscard]] bool has_mesh_shading() const
    {
        return CmdDrawMeshTasks != nullptr;
    }

    [[nodiscard]] bool has_upscaler() const
    {
        return CreateUpscaler != nullptr;
    }

    [[nodiscard]] bool has_swapchain() const
    {
        return CreateSwapChain != nullptr;
    }
};
