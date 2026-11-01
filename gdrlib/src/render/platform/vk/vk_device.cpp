#define VK_ENABLE_BETA_EXTENSIONS

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <cpp/containers/hash_set.hpp>
#include <cpp/containers/heap_array.hpp>
#include <cpp/containers/stack_string.hpp>
#include <log.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <tracy/Tracy.hpp>
#include <utils.hpp>

#include <algorithm>

using namespace platform;

#define ENABLE_SYNC_VALIDATION 0

template<typename T>
struct vk_feature_span;
#define FEATURE_SPAN(T, first, last)                                                                    \
    template<>                                                                                          \
    struct vk_feature_span<T>                                                                           \
    {                                                                                                   \
        static constexpr u64 begin = offsetof(T, first);                                                \
        static constexpr u64 count = (offsetof(T, last) + sizeof(VkBool32) - begin) / sizeof(VkBool32); \
    };                                                                                                  \
    using T##Span = vk_feature_span<T>;

FEATURE_SPAN(VkPhysicalDeviceFeatures, robustBufferAccess, inheritedQueries)
FEATURE_SPAN(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess, shaderDrawParameters)
FEATURE_SPAN(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge, subgroupBroadcastDynamicId)
FEATURE_SPAN(VkPhysicalDeviceVulkan13Features, robustImageAccess, maintenance4)
FEATURE_SPAN(VkPhysicalDeviceVulkan14Features, globalPriorityQuery, pushDescriptor)
FEATURE_SPAN(VkPhysicalDeviceMeshShaderFeaturesEXT, taskShader, meshShaderQueries)
FEATURE_SPAN(VkPhysicalDeviceShaderFloat8FeaturesEXT, shaderFloat8, shaderFloat8CooperativeMatrix)
FEATURE_SPAN(VkPhysicalDevicePortabilitySubsetFeaturesKHR, constantAlphaColorBlendFactors,
             vertexAttributeAccessBeyondStride)
#undef FEATURE_SPAN

struct vk_feature_chain
{
private:
    void** m_pNext = nullptr;
    cpp::heap_array<const char*> m_dvc_extensions;

public:
    VkPhysicalDeviceFeatures2 core {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan11Features vk11 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    VkPhysicalDeviceVulkan12Features vk12 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features vk13 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceVulkan14Features vk14 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    VkPhysicalDevicePortabilitySubsetFeaturesKHR port {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR};

    vk_feature_chain()
        : m_pNext(&core.pNext)
    {
        prepend(vk11);
        prepend(vk12);
        prepend(vk13);
        prepend(vk14);
    }

    vk_feature_chain(VkPhysicalDevice device)
        : vk_feature_chain(device, get_device_extensions(device))
    {
    }

    vk_feature_chain(VkPhysicalDevice device, const cpp::heap_array<VkExtensionProperties>& extensions)
        : vk_feature_chain()
    {
        setup_ext_query(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        setup_ext_query(extensions, VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
        setup_ext_query(extensions, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
#if !defined(__APPLE__)
        TRACY_ONLY(setup_ext_query(extensions, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME));
#endif

        setup_ext_query(mesh, extensions, VK_EXT_MESH_SHADER_EXTENSION_NAME);
        setup_ext_query(port, extensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);

        vkGetPhysicalDeviceFeatures2(device, &this->core);

        this->mesh.primitiveFragmentShadingRateMeshShader = VK_FALSE;
    }

    vk_feature_chain(const rhi::rendering_features_table& table)
        : vk_feature_chain()
    {
        init_from_requests(table);
    }

    vk_feature_chain(const vk_feature_chain&)            = delete;
    vk_feature_chain& operator=(const vk_feature_chain&) = delete;

    template<typename T>
    static std::span<VkBool32> vk_feat_flags_span(T& f)
    {
        auto* p = reinterpret_cast<u8*>(&f) + vk_feature_span<T>::begin;
        return {reinterpret_cast<VkBool32*>(p), vk_feature_span<T>::count};
    }

    template<typename T>
    static std::span<const VkBool32> vk_feat_flags_span(const T& f)
    {
        auto* p = reinterpret_cast<const u8*>(&f) + vk_feature_span<T>::begin;
        return {reinterpret_cast<const VkBool32*>(p), vk_feature_span<T>::count};
    }

    template<typename T>
    static bool supports(const T& required, const T& supported)
    {
        const auto req = vk_feat_flags_span(required);
        const auto sup = vk_feat_flags_span(supported);
        for (u64 i = 0; i < req.size(); ++i)
        {
            if (req[i] && !sup[i])
            {
                return false;
            }
        }

        return true;
    }

    static cpp::heap_array<VkExtensionProperties> get_device_extensions(VkPhysicalDevice device)
    {
        u32 ext_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);

        cpp::heap_array<VkExtensionProperties> extensions(ext_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, extensions.data());

        return extensions;
    }

    bool has_extension(std::string_view name) const
    {
        return std::ranges::any_of(this->m_dvc_extensions.begin(),
                                   this->m_dvc_extensions.end(),
                                   [&](const auto& e)
                                   {
                                       return name == e;
                                   });
    }

    static bool has_extension(const cpp::heap_array<VkExtensionProperties>& extensions, std::string_view name)
    {
        return std::ranges::any_of(extensions.begin(),
                                   extensions.end(),
                                   [&](const auto& e)
                                   {
                                       return name == e.extensionName;
                                   });
    }

    template<typename T>
    void prepend(T&& desc)
    {
        *this->m_pNext = &desc;
        this->m_pNext  = &desc.pNext;
        desc.pNext     = nullptr;
    }

    void setup_ext_query(const cpp::heap_array<VkExtensionProperties>& extensions, const char* extension)
    {
        if (has_extension(extensions, extension))
        {
            m_dvc_extensions.emplace_back(extension);
        }
    }

    template<typename T>
    void setup_ext_query(T&& desc, const cpp::heap_array<VkExtensionProperties>& extensions, const char* extension)
    {
        if (has_extension(extensions, extension))
        {
            prepend(desc);
            m_dvc_extensions.emplace_back(extension);
        }
    }

    bool satisfies(const vk_feature_chain& required) const
    {
        for (const char* extension : required.m_dvc_extensions)
        {
            if (!this->has_extension(extension))
            {
                return false;
            }
        }

        // clang-format off
        return supports(required.core.features, this->core.features)
                && supports(required.vk11, this->vk11)
                && supports(required.vk12, this->vk12)
                && supports(required.vk13, this->vk13)
                && supports(required.vk14, this->vk14)
                && supports(required.mesh, this->mesh)
                && supports(required.port, this->port);
        // clang-format on
    }

    // this is an approx. set of requirements for the device, to check if it is suitable
    // I used to compose all requirements by hand, but it was getting a bit tedious
    // so now I enable everything (that is of interest to me), and this stuff is only used to choose
    // the appropriate device.
    void init_from_requests(const rhi::rendering_features_table& table)
    {
#if !defined(__APPLE__)
        this->core.features.geometryShader = VK_TRUE;
        this->vk12.drawIndirectCount       = VK_TRUE;
#endif

        this->m_dvc_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        this->m_dvc_extensions.emplace_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
        this->m_dvc_extensions.emplace_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
#if !defined(__APPLE__) && TRACY_ENABLE
        this->m_dvc_extensions.emplace_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
#endif

        this->core.features.samplerAnisotropy                    = VK_TRUE;
        this->core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        this->core.features.multiDrawIndirect                    = VK_TRUE;
        this->core.features.depthClamp                           = VK_TRUE;
        this->core.features.samplerAnisotropy                    = VK_TRUE;
        this->core.features.shaderInt16                          = VK_TRUE;
        this->core.features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        this->core.features.pipelineStatisticsQuery              = table.required(rhi::feature_flag::pipeline_stats);

        this->vk11.shaderDrawParameters               = VK_TRUE;
        this->vk11.storageBuffer16BitAccess           = VK_TRUE;
        this->vk11.uniformAndStorageBuffer16BitAccess = VK_TRUE;

#if TRACY_ENABLE
        this->vk12.hostQueryReset = VK_TRUE;
#endif
        this->vk12.storageBuffer8BitAccess                      = VK_TRUE;
        this->vk12.shaderFloat16                                = VK_TRUE;
        this->vk12.descriptorIndexing                           = VK_TRUE;
        this->vk12.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
        this->vk12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        this->vk12.descriptorBindingPartiallyBound              = VK_TRUE;
        this->vk12.descriptorBindingVariableDescriptorCount     = VK_TRUE;
        this->vk12.runtimeDescriptorArray                       = VK_TRUE;
        this->vk12.scalarBlockLayout                            = VK_TRUE;
        this->vk12.timelineSemaphore                            = VK_TRUE;
        this->vk12.samplerFilterMinmax                          = table.required(rhi::feature_flag::sampler_min_max),

        this->vk13.synchronization2               = VK_TRUE;
        this->vk13.dynamicRendering               = VK_TRUE;
        this->vk13.maintenance4                   = VK_TRUE;
        this->vk13.shaderDemoteToHelperInvocation = VK_TRUE;

        this->vk14.maintenance5   = VK_TRUE;
        this->vk14.maintenance6   = VK_TRUE;
        this->vk14.pushDescriptor = VK_TRUE;

        if (!table.required(rhi::feature_flag::validation))
        {
            this->vk11.protectedMemory             = VK_FALSE;
            this->vk13.robustImageAccess           = VK_FALSE;
            this->core.features.robustBufferAccess = VK_FALSE;
        }

        if (table.required(rhi::feature_flag::mesh_shading))
        {
            this->mesh = {
                .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
                .taskShader = VK_TRUE,
                .meshShader = VK_TRUE,
            };
            prepend(this->mesh);
            this->m_dvc_extensions.emplace_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }

        if (table.required(rhi::feature_flag::portability_subset))
        {
            this->port = {
                .sType                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
                .mutableComparisonSamplers = VK_TRUE,
            };
            prepend(this->port);
            this->m_dvc_extensions.emplace_back("VK_KHR_portability_subset");
        }
    }

    u32 dvc_extensions_count() const { return m_dvc_extensions.size(); }

    const char* const* dvc_extensions() const { return m_dvc_extensions.data(); }
};

static const char* device_type_to_str(VkPhysicalDeviceType type)
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU :
        return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU :
        return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU :
        return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU :
        return "cpu";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER :
    default :
        return "other";
    }
}

static bool inst_ext_available(const char* name)
{
    u32 n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    cpp::heap_array<VkExtensionProperties> props(n);
    if (n)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &n, props.data());
    }
    for (auto& p : props)
    {
        if (cpp::cx_streq(p.extensionName, name))
        {
            return true;
        }
    }
    return false;
}

static bool layer_available(const char* name)
{
    u32 n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    cpp::heap_array<VkLayerProperties> props(n);

    if (n)
    {
        vkEnumerateInstanceLayerProperties(&n, props.data());
    }

    for (auto& p : props)
    {
        if (cpp::cx_streq(p.layerName, name))
        {
            return true;
        }
    }

    return false;
}

static void insert_video_driver_extensions(const window& window, cpp::heap_array<const char*>& extensions_array)
{
    extensions_array.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    switch (window.get_native_handle().type)
    {
    case window::driver_type::windows :
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        extensions_array.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
        break;
    case window::driver_type::x11 :
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        extensions_array.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
        break;
    case window::driver_type::wayland :
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        extensions_array.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
        break;
    case window::driver_type::metal :
#if defined(VK_USE_PLATFORM_METAL_EXT)
        extensions_array.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
        extensions_array.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
        break;
    default :
        break;
    }
}

static VkResult video_driver_create_surface(const window& window, VkInstance instance, VkSurfaceKHR* surface)
{
    ZoneScoped;
    const auto native = window.get_native_handle();
    switch (native.type)
    {
    case window::driver_type::windows :
    {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VkWin32SurfaceCreateInfoKHR win32_create_info {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hwnd  = static_cast<HWND>(native.windows.hwnd),
        };

        return vkCreateWin32SurfaceKHR(instance, &win32_create_info, nullptr, surface);
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    }
    case window::driver_type::x11 :
    {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        VkXlibSurfaceCreateInfoKHR xlib_create_info = {
            .sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
            .dpy    = (::Display*)native.x11.display,
            .window = (::Window)native.x11.window,
        };

        return vkCreateXlibSurfaceKHR(instance, &xlib_create_info, nullptr, surface);
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    }
    case window::driver_type::wayland :
    {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VkWaylandSurfaceCreateInfoKHR wayland_create_info = {
            .sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (wl_display*)native.wayland.display,
            .surface = (wl_surface*)native.wayland.surface,
        };

        return vkCreateWaylandSurfaceKHR(instance, &wayland_create_info, nullptr, surface);
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    }
    case window::driver_type::metal :
    {
#if defined(VK_USE_PLATFORM_METAL_EXT)
        VkMetalSurfaceCreateInfoEXT metal_create_info = {.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
                                                         .pLayer = static_cast<CAMetalLayer*>(native.metal.ca_layer)};

        return vkCreateMetalSurfaceEXT(instance, &metal_create_info, nullptr, surface);
#else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
    }
    default :
        break;
    }

    return VK_ERROR_FEATURE_NOT_PRESENT;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                               VkDebugUtilsMessageTypeFlagsEXT,
                                               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    if (!pCallbackData->pMessage)
    {
        return VK_FALSE;
    }

    constexpr const char* kFilterPhrases[] = {
        // FSR 4.1.2 hidden requirements or errors
        "SPIR-V Extension SPV_EXT_float8 was declared",
        "SPIR-V Extension SPV_NV_cooperative_matrix2 was declared",
        "SPIR-V Extension SPV_KHR_cooperative_matrix was declared",

        "SPIR-V Capability CooperativeMatrixKHR was declared",
        "SPIR-V Capability Float8CooperativeMatrixEXT was declared",
        "SPIR-V Capability CooperativeMatrixConversionsNV was declared",

        "Expected Use of Matrix type and Result Type to be identical",
        "Cannot store to OpTypeImage, OpTypeSampler, OpTypeSampledImage, or OpTypeAccelerationStructureKHR objects",
        "OpImage format of VK_FORMAT_R16G16B16A16_SFLOAT which has 4 components, but the OpImageWrite Texel operand only contains 3 components",
    };

    std::string_view msg(pCallbackData->pMessage);
    const bool filtered_out = std::ranges::any_of(kFilterPhrases,
                                                  [&](const char* p)
                                                  {
                                                      return msg.find(p) != std::string_view::npos;
                                                  });

    if (filtered_out)
    {
        LOG_INFO("Filtered-out validation message: {}", pCallbackData->pMessage);
        return VK_FALSE;
    }

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOG_ERROR("Validation error: {}", msg);
        assert2m(false, pCallbackData->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOG_WARNING("Validation warning: {}", pCallbackData->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOG_INFO("Validation message: {}", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

static void load_instance_layers_and_extensions(const window& window, rhi::rendering_features_table& desc,
                                                cpp::heap_array<const char*>& layers,
                                                cpp::heap_array<const char*>& extensions)
{
    if (desc.requested(rhi::feature_flag::validation) && layer_available("VK_LAYER_KHRONOS_validation")
        && inst_ext_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        desc.set_supported(rhi::feature_flag::validation, true);
        desc.set_supported(rhi::feature_flag::debug_labels, true);

        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_DEBUG("Validation was requested and seems to be available (VK_LAYER_KHRONOS_validation)");
    }
    else if (desc.requested(rhi::feature_flag::debug_labels) && inst_ext_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        desc.set_supported(rhi::feature_flag::debug_labels, true);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        LOG_DEBUG("Labels were requested and will be enabled");
    }

    insert_video_driver_extensions(window, extensions);
}

static bool choose_queue_families(VkPhysicalDevice phys, VkSurfaceKHR surface, u32& gfx, u32& cmp, u32& transfer,
                                  u32& present)
{
    ZoneScoped;
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);

    cpp::heap_array<VkQueueFamilyProperties> qfp(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, qfp.data());

    auto pick_any_pred = [&](auto pred) -> u32
    {
        for (u32 i = 0; i < count; i++)
        {
            if (pred(i))
            {
                return i;
            }
        }

        return VK_QUEUE_FAMILY_IGNORED;
    };

    auto pick_any = [&](auto flags)
    {
        return pick_any_pred(
            [&](u32 id) -> bool
            {
                return qfp[id].queueFlags & flags;
            });
    };

    // Compute: prefer compute-only queue (no graphics), else any compute
    for (u32 i = 0; i < count; i++)
    {
        const auto flags = qfp[i].queueFlags;
        if ((flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT))
        {
            cmp = i;
            break;
        }
    }
    // Copy: prefer transfer-only (neither graphics nor compute), else any transfer
    for (u32 i = 0; i < count; i++)
    {
        const auto flags = qfp[i].queueFlags;
        if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && !(flags & VK_QUEUE_COMPUTE_BIT))
        {
            transfer = i;
            break;
        }
    }

    gfx      = pick_any(VK_QUEUE_GRAPHICS_BIT);
    cmp      = cmp == VK_QUEUE_FAMILY_IGNORED ? pick_any(VK_QUEUE_COMPUTE_BIT) : cmp;
    transfer = transfer == VK_QUEUE_FAMILY_IGNORED ? pick_any(VK_QUEUE_TRANSFER_BIT) : transfer;
    present  = pick_any_pred(
        [&](auto id) -> bool
        {
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(phys, id, surface, &present_support);

            return present_support;
        });

    return present != VK_QUEUE_FAMILY_IGNORED && gfx != VK_QUEUE_FAMILY_IGNORED && cmp != VK_QUEUE_FAMILY_IGNORED
        && transfer != VK_QUEUE_FAMILY_IGNORED;
}

static cpp::heap_array<VkDeviceQueueCreateInfo> get_queue_create_info(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    ZoneScoped;
    // 1st - gfx, 2nd - compute, 3rd - transfer.
    u32 families[rhi::kQueueTypesCount];
    std::fill(families, std::end(families), VK_QUEUE_FAMILY_IGNORED);

    if (!choose_queue_families(device, surface, families[0], families[1], families[2], families[3]))
    {
        return {};
    }

    std::ranges::sort(families);

    u32 unique[rhi::kQueueTypesCount];
    u32 unique_count = 0;

    for (u32 i = 0; i < rhi::kQueueTypesCount; i++)
    {
        if (i == 0 || families[i] != families[i - 1])
        {
            unique[unique_count++] = families[i];
        }
    }

    constexpr static float kPriority = 1.0f;
    cpp::heap_array<VkDeviceQueueCreateInfo> create_infos(unique_count);

    for (u32 i = 0; i < unique_count; i++)
    {
        create_infos[i] = {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique[i],
            .queueCount       = 1,
            .pQueuePriorities = &kPriority,
        };
    }

    return create_infos;
}

static bool check_device_basic_features_support(VkPhysicalDevice device, VkSurfaceKHR surface,
                                                rhi::rendering_features_table& features_table)
{
    ZoneScoped;
    u32 families[rhi::kQueueTypesCount];
    std::fill(families, std::end(families), VK_QUEUE_FAMILY_IGNORED);

    choose_queue_families(device, surface, families[0], families[1], families[2], families[3]);

    const auto incomplete = std::ranges::any_of(families,
                                                [](auto id)
                                                {
                                                    return id == VK_QUEUE_FAMILY_IGNORED;
                                                });

    if (incomplete)
    {
        return false;
    }

    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);

    if (format_count == 0 || present_mode_count == 0)
    {
        return false;
    }

    u32 device_extensions_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extensions_count, nullptr);

    cpp::heap_array<VkExtensionProperties> device_extensions(device_extensions_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extensions_count, device_extensions.data());

    const vk_feature_chain supported(device, device_extensions);
    features_table.set_supported(rhi::feature_flag::mesh_shading,
                                 supported.mesh.meshShader && supported.mesh.taskShader);

    features_table.set_supported(rhi::feature_flag::sampler_min_max, supported.vk12.samplerFilterMinmax);
    features_table.set_supported(rhi::feature_flag::pipeline_stats, supported.core.features.pipelineStatisticsQuery);

    const vk_feature_chain required(features_table);
    return supported.satisfies(required) && features_table.all_required_supported();
}

static u32 rate_device(VkPhysicalDevice physical_device)
{
    ZoneScoped;
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);

    u32 score = 1;
    score += (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 1000;

    return score;
}

static void log_device_features_table(const char* name, const rhi::rendering_features_table& feat_table)
{
    LOG_DEBUG("{} features report:", name);
    for (u32 i = 0; i < reflection::get_enum_values_count<rhi::feature_flag>() - 1; ++i)
    {
        const auto flag = reflection::get_enum_value_at<rhi::feature_flag>(i);

        // validation is an instance-level flag tbh
        if (flag == rhi::feature_flag::validation)
        {
            continue;
        }

        const bool supported = feat_table.supported(flag);
        const bool wanted    = feat_table.requested(flag) || feat_table.required(flag);

        if (supported || !wanted)
        {
            LOG_DEBUG(
                "{}: {}", reflection::get_enum_name_at<rhi::feature_flag>(i), supported ? "supported" : "unsupported");
        }
        else if (wanted)
        {
            LOG_WARNING("{}: feature requested but unsupported", reflection::get_enum_name_at<rhi::feature_flag>(i));
        }
    }
}

static VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface,
                                             rhi::rendering_features_table& required_features, u32 wanted_index)
{
    ZoneScoped;
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    cpp::heap_array<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    u32 best_rating                                       = 0;
    VkPhysicalDevice current_pick                         = VK_NULL_HANDLE;
    rhi::rendering_features_table device_features_support = required_features;

    for (u32 i = 0; i < device_count; ++i)
    {
        const auto device = devices[i];

        VkPhysicalDeviceDriverProperties driver_properties {.sType =
                                                                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                 .pNext = &driver_properties};
        vkGetPhysicalDeviceProperties2(device, &properties2);

        LOG_DEBUG("Evaluating device #{}: {} ({}). Driver: {} ({})",
                  i,
                  properties2.properties.deviceName,
                  device_type_to_str(properties2.properties.deviceType),
                  driver_properties.driverName,
                  driver_properties.driverInfo);

        if (!check_device_basic_features_support(device, surface, required_features))
        {
            LOG_DEBUG("Device #{} does not support all required features", i);
            log_device_features_table(properties2.properties.deviceName, required_features);
            continue;
        }

        if (wanted_index == i)
        {
            current_pick            = device;
            device_features_support = required_features;
            LOG_DEBUG("Device #{} selected according to device id hint", i);
            break;
        }

        const auto rating = rate_device(device);
        if (rating > best_rating)
        {
            LOG_DEBUG("Device #{} has the best rating ({} > {}) and is the best so far", i, rating, best_rating);

            best_rating             = rating;
            current_pick            = device;
            device_features_support = required_features;
        }
    }

    if (current_pick != VK_NULL_HANDLE)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(current_pick, &properties);

        LOG_INFO("selected physical device: {}", properties.deviceName);
        log_device_features_table(properties.deviceName, device_features_support);
    }

    assert2m(current_pick != VK_NULL_HANDLE, "No suitable physical device found");
    required_features = device_features_support;
    return current_pick;
}

static VkResult create_vma_allocator(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device,
                                     VmaAllocator* allocator)
{
    ZoneScoped;
    const VmaVulkanFunctions vma_vulkan_functions = {
        .vkGetInstanceProcAddr                   = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr                     = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory                        = vkAllocateMemory,
        .vkFreeMemory                            = vkFreeMemory,
        .vkMapMemory                             = vkMapMemory,
        .vkUnmapMemory                           = vkUnmapMemory,
        .vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory                      = vkBindBufferMemory,
        .vkBindImageMemory                       = vkBindImageMemory,
        .vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements,
        .vkCreateBuffer                          = vkCreateBuffer,
        .vkDestroyBuffer                         = vkDestroyBuffer,
        .vkCreateImage                           = vkCreateImage,
        .vkDestroyImage                          = vkDestroyImage,
        .vkCmdCopyBuffer                         = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR                  = vkBindBufferMemory2,
        .vkBindImageMemory2KHR                   = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
        .vkGetDeviceBufferMemoryRequirements     = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements      = vkGetDeviceImageMemoryRequirements,
        .vkGetMemoryWin32HandleKHR               = nullptr,
    };

#if TRACY_ENABLE
    constexpr const char* kPlotName = "GPU memory";

    struct vma_alloc_cb_data
    {
        u64 alloc_total_bytes = 0;
    };

    const PFN_vmaAllocateDeviceMemoryFunction alloc_callback =
        +[](VmaAllocator VMA_NOT_NULL /* allocator */,
            uint32_t /* memoryType */,
            VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE /* memory */,
            const VkDeviceSize size,
            void* VMA_NULLABLE pUserData)
    {
        if (pUserData)
        {
            auto& data = *static_cast<vma_alloc_cb_data*>(pUserData);
            data.alloc_total_bytes += size;
            const auto msg =
                cpp::stack_string::make_formatted("VMA alloc: %s (+%s)",
                                                  utils::format_bytes_number(data.alloc_total_bytes).c_str(),
                                                  utils::format_bytes_number(size).c_str());
            TracyMessage(msg.c_str(), msg.length());

            TracyPlotConfig(kPlotName, tracy::PlotFormatType::Memory, true, true, 0);
            TracyPlot(kPlotName, static_cast<i64>(data.alloc_total_bytes));
            LOG_DEBUG("{}", msg.c_str());
        }
    };

    const PFN_vmaAllocateDeviceMemoryFunction free_callback =
        +[](VmaAllocator VMA_NOT_NULL /* allocator */,
            uint32_t /* memoryType */,
            VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE /* memory */,
            const VkDeviceSize size,
            void* VMA_NULLABLE pUserData)
    {
        if (pUserData)
        {
            auto& data = *static_cast<vma_alloc_cb_data*>(pUserData);
            data.alloc_total_bytes -= size;
            const auto msg =
                cpp::stack_string::make_formatted("VMA alloc: %s (-%s)",
                                                  utils::format_bytes_number(data.alloc_total_bytes).c_str(),
                                                  utils::format_bytes_number(size).c_str());
            TracyMessage(msg.c_str(), msg.length());

            TracyPlotConfig(kPlotName, tracy::PlotFormatType::Memory, true, true, 0);
            TracyPlot(kPlotName, static_cast<i64>(data.alloc_total_bytes));
            LOG_DEBUG("{}", msg.c_str());
        }
    };

    static vma_alloc_cb_data s_vma_cb_data;

    const VmaDeviceMemoryCallbacks callbacks {
        .pfnAllocate = alloc_callback, .pfnFree = free_callback, .pUserData = &s_vma_cb_data};
#endif

    const VmaAllocatorCreateInfo allocator_create_info = {
        .flags          = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        .physicalDevice = physical_device,
        .device         = device,
#if TRACY_ENABLE
        .pDeviceMemoryCallbacks = &callbacks,
#endif
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance         = instance,
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };

    return vmaCreateAllocator(&allocator_create_info, allocator);
}

static VkResult create_vulkan_device(VkPhysicalDevice phys_device, VkSurfaceKHR surface, VkDevice* device)
{
    const vk_feature_chain features(phys_device);
    const auto queue_create_info = get_queue_create_info(phys_device, surface);

    const VkDeviceCreateInfo device_create_info {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &features.core,
        .queueCreateInfoCount    = static_cast<u32>(queue_create_info.size()),
        .pQueueCreateInfos       = queue_create_info.data(),
        .enabledExtensionCount   = features.dvc_extensions_count(),
        .ppEnabledExtensionNames = features.dvc_extensions(),
        .pEnabledFeatures        = nullptr,
    };

    return vkCreateDevice(phys_device, &device_create_info, nullptr, device);
}

static result<vk_context> create_vk_context(const window& window, const rhi::instance_desc& desc)
{
    ZoneScoped;
    VK_RETURN_ON_FAIL(volkInitialize())
    vk_context context {};

    // Application info
    VkApplicationInfo application_info {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application_info.pApplicationName = desc.app_name ? desc.app_name : "NoName";
    application_info.pEngineName      = desc.app_name ? desc.app_name : "NoName";
    application_info.engineVersion    = 1;
    application_info.apiVersion       = VK_API_VERSION_1_4;

    // copy requested & required features
    context.enabled_device_features = desc.device_features;

    cpp::heap_array<const char*> layers;
    load_instance_layers_and_extensions(window, context.enabled_device_features, layers, context.instance_extensions);

#if ENABLE_SYNC_VALIDATION
    constexpr VkValidationFeatureEnableEXT enabled_validation_features[] = {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };

    VkValidationFeaturesEXT validation_features       = {VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validation_features.enabledValidationFeatureCount = COUNT_OF(enabled_validation_features);
    validation_features.pEnabledValidationFeatures    = enabled_validation_features;
#endif

    const VkInstanceCreateInfo ici {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if ENABLE_SYNC_VALIDATION
        .pNext = &validation_features,
#endif
#if defined(VK_USE_PLATFORM_METAL_EXT)
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
#endif
        .pApplicationInfo        = &application_info,
        .enabledLayerCount       = static_cast<u32>(layers.size()),
        .ppEnabledLayerNames     = layers.empty() ? nullptr : layers.data(),
        .enabledExtensionCount   = static_cast<u32>(context.instance_extensions.size()),
        .ppEnabledExtensionNames = context.instance_extensions.empty() ? nullptr : context.instance_extensions.data()};

    if (volkGetInstanceVersion() < ici.pApplicationInfo->apiVersion)
    {
        return error("Requested API version is not supported by an instance");
    }

    VK_RETURN_ON_FAIL(vkCreateInstance(&ici, nullptr, &context.instance));
    volkLoadInstance(context.instance);

    // Create debug messenger if available and requested
    if (vkCreateDebugUtilsMessengerEXT && desc.device_features.requested(rhi::feature_flag::validation))
    {
        constexpr VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_cb};

        VK_RETURN_ON_FAIL(vkCreateDebugUtilsMessengerEXT(
            context.instance, &debug_messenger_create_info, nullptr, &context.debug_messenger));
    }

    VK_RETURN_ON_FAIL(video_driver_create_surface(window, context.instance, &context.surface));

    // here context.enabled_device_features is populated with all supported features, both required and requested
    context.physical_device =
        pick_physical_device(context.instance, context.surface, context.enabled_device_features, desc.device_id_hint);

    // build new extensions table with required + (requested & supported) extensions included
    VK_RETURN_ON_FAIL(create_vulkan_device(context.physical_device, context.surface, &context.device));
    VK_RETURN_ON_FAIL(
        create_vma_allocator(context.instance, context.device, context.physical_device, &context.allocator));

    choose_queue_families(context.physical_device,
                          context.surface,
                          context.queues[static_cast<u32>(rhi::queue_kind::gfx)].family,
                          context.queues[static_cast<u32>(rhi::queue_kind::compute)].family,
                          context.queues[static_cast<u32>(rhi::queue_kind::transfer)].family,
                          context.queues[static_cast<u32>(rhi::queue_kind::present)].family);

    auto populate_queue_data = [&](const u32 idx)
    {
        vkGetDeviceQueue(context.device, context.queues[idx].family, 0, &context.queues[idx].queue);
    };

    for (u32 i = 0; i < rhi::kQueueTypesCount; i++)
    {
        populate_queue_data(i);
    }

#if TRACY_ENABLE
    context.tracy_ctx = TracyVkContextHostCalibrated(context.physical_device,
                                                     context.device,
                                                     vkResetQueryPool,
                                                     vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
                                                     vkGetCalibratedTimestampsEXT);
#endif

    return context;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D requested)
{
    return {.width  = std::clamp(requested.width, caps.minImageExtent.width, caps.maxImageExtent.width),
            .height = std::clamp(requested.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
}

static VkPresentModeKHR choose_present_mode(VkPhysicalDevice device, VkSurfaceKHR surface, bool vsync)
{
    ZoneScoped;
    if (vsync)
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    u32 present_modes = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes, nullptr);

    cpp::heap_array<VkPresentModeKHR> available_present_modes(present_modes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes, available_present_modes.data());

    for (const auto& present_mode : available_present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR choose_swapchain_format(VkPhysicalDevice device, VkSurfaceKHR surface, VkFormat format)
{
    ZoneScoped;
    u32 surface_formats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats, nullptr);

    cpp::heap_array<VkSurfaceFormatKHR> available_formats(surface_formats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats, available_formats.data());

    auto choose_format_pred = [&](auto pred) -> VkSurfaceFormatKHR
    {
        for (const auto& avail_vk_format : available_formats)
        {
            if (pred(avail_vk_format))
            {
                return avail_vk_format;
            }
        }

        return VkSurfaceFormatKHR {.format = VK_FORMAT_UNDEFINED};
    };

    VkSurfaceFormatKHR formats[3];
    formats[0] = choose_format_pred(
        [target_format = format](auto vk_format)
        {
            return target_format == vk_format.format;
        });
    // Fallback 1
    formats[1] = choose_format_pred(
        [](auto vk_format)
        {
            return vk_format.format == VK_FORMAT_B8G8R8A8_SRGB
                && vk_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
    // Fallback 2
    formats[2] = choose_format_pred(
        [](auto vk_format)
        {
            return vk_format.format == VK_FORMAT_B8G8R8A8_SRGB;
        });

    for (auto candidate : formats)
    {
        if (candidate.format != VK_FORMAT_UNDEFINED)
        {
            return candidate;
        }
    }

    return available_formats[0];
}

void platform::vk_destroy_swapchain(const vk_context& vk_context, vk_swapchain& swapchain)
{
    ZoneScoped;
    VK_DO_IF_NOT_NULL(vk_context.device, vkDeviceWaitIdle(vk_context.device));

    if (swapchain.sc != VK_NULL_HANDLE)
    {
        for (auto& img : swapchain.images)
        {
            VK_DESTROY(img.image.view, vkDestroyImageView, vk_context.device);
            VK_DESTROY(img.release_semaphore, vkDestroySemaphore, vk_context.device);
        }

        VK_DESTROY(swapchain.sc, vkDestroySwapchainKHR, vk_context.device);
    }

    swapchain.sc = VK_NULL_HANDLE;
    swapchain.images.clear();
}

result<vk_swapchain> platform::vk_create_swapchain(const vk_context& vk_context, VkFormat format, ivec2 size,
                                                   u32 frames_in_flight, bool vsync, const vk_swapchain* old_swapchain)
{
    ZoneScoped;
    vk_swapchain sc_data;
    sc_data.surface_format = choose_swapchain_format(vk_context.physical_device, vk_context.surface, format);

    const u32 unique_queues[2] {vk_context.queues[static_cast<u32>(rhi::queue_kind::gfx)].family,
                                vk_context.queues[static_cast<u32>(rhi::queue_kind::present)].family};

    const bool image_sharing_supported = vk_context.queues[static_cast<u32>(rhi::queue_kind::gfx)].family
                                      != vk_context.queues[static_cast<u32>(rhi::queue_kind::present)].family;

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context.physical_device, vk_context.surface, &capabilities);

    const VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext   = nullptr,
        .flags   = 0,
        .surface = vk_context.surface,
        .minImageCount =
            std::clamp(frames_in_flight,
                       capabilities.minImageCount,
                       capabilities.maxImageCount > 0 ? capabilities.maxImageCount
                                                      : (std::max(frames_in_flight, capabilities.minImageCount))),
        .imageFormat      = sc_data.surface_format.format,
        .imageColorSpace  = sc_data.surface_format.colorSpace,
        .imageExtent      = choose_extent(capabilities, {static_cast<u32>(size.x), static_cast<u32>(size.y)}),
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode      = image_sharing_supported ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = image_sharing_supported ? static_cast<u32>(COUNT_OF(unique_queues)) : 0,
        .pQueueFamilyIndices   = image_sharing_supported ? unique_queues : nullptr,
        .preTransform          = capabilities.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = choose_present_mode(vk_context.physical_device, vk_context.surface, vsync),
        .clipped               = VK_TRUE,
        .oldSwapchain          = old_swapchain ? old_swapchain->sc : VK_NULL_HANDLE,
    };

    VK_RETURN_ON_FAIL(vkCreateSwapchainKHR(vk_context.device, &swapchain_create_info, nullptr, &sc_data.sc));

    u32 img_count = 0;
    vkGetSwapchainImagesKHR(vk_context.device, sc_data.sc, &img_count, nullptr);

    cpp::heap_array<VkImage> imgs(img_count);
    vkGetSwapchainImagesKHR(vk_context.device, sc_data.sc, &img_count, imgs.data());

    for (u32 i = 0; i < img_count; i++)
    {
        VkImageViewCreateInfo image_view_create_info {
            .sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image      = imgs[i],
            .viewType   = VK_IMAGE_VIEW_TYPE_2D,
            .format     = sc_data.surface_format.format,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange =
                {
                           .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel   = 0,
                           .levelCount     = 1,
                           .baseArrayLayer = 0,
                           .layerCount     = 1,
                           },
        };

        vk_swapchain_image sc_image {};
        sc_image.image.image = imgs[i];

        VK_RETURN_ON_FAIL(vkCreateImageView(vk_context.device, &image_view_create_info, nullptr, &sc_image.image.view));

        constexpr VkSemaphoreCreateInfo semaphore_create_info {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_ASSERT_ON_FAIL(
            vkCreateSemaphore(vk_context.device, &semaphore_create_info, nullptr, &sc_image.release_semaphore));

        sc_data.images.emplace_back(sc_image);
    }

    return sc_data;
}

void platform::vk_destroy_context(vk_context& ctx)
{
    ZoneScoped;

    ctx.instance_extensions.clear();

#if TRACY_ENABLE
    TracyVkDestroy(ctx.tracy_ctx);
#endif

    VK_DO_IF_NOT_NULL(ctx.device, vkDeviceWaitIdle(ctx.device));
    VK_DO_IF_NOT_NULL(ctx.allocator, vmaDestroyAllocator(ctx.allocator));

    VK_DESTROY(ctx.surface, vkDestroySurfaceKHR, ctx.instance);
    VK_DESTROY(ctx.debug_messenger, vkDestroyDebugUtilsMessengerEXT, ctx.instance);

    VK_DO_IF_NOT_NULL(ctx.device, vkDestroyDevice(ctx.device, nullptr));
    VK_DO_IF_NOT_NULL(ctx.instance, vkDestroyInstance(ctx.instance, nullptr));
}

result<vk_context> platform::vk_create_context(const window& window, const rhi::instance_desc& instance_desc)
{
    ZoneScoped;
    auto r_created_context = create_vk_context(window, instance_desc);

    RESULT_FORWARD_IF_FAILED(r_created_context);
    return *r_created_context;
}
