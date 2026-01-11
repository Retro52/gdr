#include <cpp/alg_constexpr.hpp>
#include <cpp/containers/stack_string.hpp>
#include <render/platform/vk/vk_device.hpp>
#include <render/platform/vk/vk_error.hpp>
#include <Tracy/Tracy.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <ranges>
#include <unordered_set>
#include <vector>

using namespace render;

#define VK_DO_IF_NOT_NULL(VK_OBJ, EXPR) \
    if ((VK_OBJ) != VK_NULL_HANDLE)     \
    {                                   \
        EXPR;                           \
    }

bool inst_ext_available(const char* name)
{
    u32 n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> props(n);
    if (n)
    {
        vkEnumerateInstanceExtensionProperties(nullptr, &n, props.data());
    }
    for (auto& p : props)
    {
        if (std::strcmp(p.extensionName, name) == 0)
        {
            return true;
        }
    }
    return false;
}

bool layer_available(const char* name)
{
    u32 n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> props(n);

    if (n)
    {
        vkEnumerateInstanceLayerProperties(&n, props.data());
    }

    for (auto& p : props)
    {
        if (std::strcmp(p.layerName, name) == 0)
        {
            return true;
        }
    }

    return false;
}

void insert_video_driver_extensions(const window& window, std::vector<const char*>& extensions_array)
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

VkResult video_driver_create_surface(const window& window, VkInstance instance, VkSurfaceKHR* surface)
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

VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
{
    std::cerr << "[VK VALIDATION] ";
    std::cerr << (pCallbackData->pMessage ? pCallbackData->pMessage : "(null)");
    std::cerr << std::endl;

    if (pCallbackData->flags & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << std::flush;
        assert(false && "vk validation error");
    }
    return VK_FALSE;
}

void load_instance_layers_and_extensions(const window& window, const instance_desc& desc,
                                         std::vector<const char*>& layers, std::vector<const char*>& extensions)
{
    if (desc.device_features.required(rendering_features_table::eValidation)
        && layer_available("VK_LAYER_KHRONOS_validation") && inst_ext_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
    {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    insert_video_driver_extensions(window, extensions);
}

bool choose_queue_families(VkPhysicalDevice phys, VkSurfaceKHR surface, u32& gfx, u32& cmp, u32& transfer, u32& present)
{
    ZoneScoped;
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);

    std::vector<VkQueueFamilyProperties> qfp(count);
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

std::vector<VkDeviceQueueCreateInfo> get_queue_create_info(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    ZoneScoped;
    // 1st - gfx, 2nd - compute, 3rd - transfer.
    u32 families[queue_kind::COUNT];
    std::fill(families, std::end(families), VK_QUEUE_FAMILY_IGNORED);

    if (!choose_queue_families(device, surface, families[0], families[1], families[2], families[3]))
    {
        return {};
    }

    std::ranges::sort(families);

    u32 unique[queue_kind::COUNT];
    u32 unique_count = 0;

    for (u32 i = 0; i < queue_kind::COUNT; i++)
    {
        if (i == 0 || families[i] != families[i - 1])
        {
            unique[unique_count++] = families[i];
        }
    }

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> create_infos(unique_count);

    for (u32 i = 0; i < unique_count; i++)
    {
        create_infos[i] = {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique[i],
            .queueCount       = 1,
            .pQueuePriorities = &priority,
        };
    }

    return create_infos;
}

bool check_device_basic_features_support(VkPhysicalDevice device, VkSurfaceKHR surface,
                                         const ext_array& required_extensions,
                                         const rendering_features_table& required_features)
{
    ZoneScoped;
    u32 families[queue_kind::COUNT];
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

    std::vector<VkExtensionProperties> device_extensions(device_extensions_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extensions_count, device_extensions.data());

    std::unordered_set<std::string_view> not_found_extensions {required_extensions.begin(), required_extensions.end()};
    for (const auto& extension : device_extensions)
    {
        not_found_extensions.erase(extension.extensionName);
    }

    VkPhysicalDeviceVulkan13Features vk13_features {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

    VkPhysicalDeviceVulkan12Features vk12_features {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                    .pNext = &vk13_features};

    VkPhysicalDeviceVulkan11Features vk11_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vk12_features,
    };

    VkPhysicalDeviceFeatures2 device_features2 {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                                .pNext = &vk11_features};
    vkGetPhysicalDeviceFeatures2(device, &device_features2);

    const bool features_supported =
        cpp::cx_implies(required_features.required(rendering_features_table::eDrawIndirect),
                        vk11_features.shaderDrawParameters)
        && cpp::cx_implies(required_features.required(rendering_features_table::eDrawIndirect),
                           device_features2.features.multiDrawIndirect)
        && cpp::cx_implies(required_features.required(rendering_features_table::eMeshShading),
                           vk12_features.storageBuffer8BitAccess)
        && cpp::cx_implies(required_features.required(rendering_features_table::eMeshShading),
                           vk13_features.maintenance4)
        && cpp::cx_implies(required_features.required(rendering_features_table::eDynamicRender),
                           vk13_features.dynamicRendering)
        && cpp::cx_implies(required_features.required(rendering_features_table::eSynchronization2),
                           vk13_features.synchronization2);

    return not_found_extensions.empty() && features_supported;
}

u32 rate_device(VkPhysicalDevice physical_device)
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

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, const ext_array& required_extensions,
                                      const rendering_features_table& required_features)
{
    ZoneScoped;
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    u32 best_rating               = 0;
    VkPhysicalDevice current_pick = VK_NULL_HANDLE;

    for (const auto device : devices)
    {
        const auto rating = rate_device(device);
        if (check_device_basic_features_support(device, surface, required_extensions, required_features)
            && rating > best_rating)
        {
            best_rating  = rating;
            current_pick = device;
        }
    }

    return current_pick;
}

VkResult create_vma_allocator(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device,
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

    const VmaAllocatorCreateInfo allocator_create_info = {
        .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
        //        .flags            = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT |
        //        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice   = physical_device,
        .device           = device,
        .pVulkanFunctions = &vma_vulkan_functions,
        .instance         = instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    return vmaCreateAllocator(&allocator_create_info, allocator);
}

result<context> create_vk_context(const window& window, const instance_desc& desc)
{
    ZoneScoped;
    VK_RETURN_ON_FAIL(volkInitialize())
    context context {};

    // Application info
    VkApplicationInfo application_info {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application_info.pApplicationName   = desc.app_name ? desc.app_name : "NoName";
    application_info.applicationVersion = desc.app_version;
    application_info.pEngineName        = desc.app_name ? desc.app_name : "NoName";
    application_info.engineVersion      = 1;
    application_info.apiVersion         = VK_API_VERSION_1_3;

    std::vector<const char*> layers;
    load_instance_layers_and_extensions(window, desc, layers, context.instance_extensions);

    const VkInstanceCreateInfo ici {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
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
        return "Requested API version is not supported by an instance";
    }

    VK_RETURN_ON_FAIL(vkCreateInstance(&ici, nullptr, &context.instance));
    volkLoadInstance(context.instance);

    // Create debug messenger if available and requested
    if (desc.device_features.required(rendering_features_table::eValidation))
    {
        const VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info {
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

    context.enabled_device_features   = desc.device_features;
    context.enabled_device_extensions = desc.device_extensions;
    context.physical_device =
        pick_physical_device(context.instance, context.surface, desc.device_extensions, desc.device_features);

    const auto queue_create_info = get_queue_create_info(context.physical_device, context.surface);

    VkPhysicalDeviceVulkan13Features vk13_features {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = desc.device_features.required(rendering_features_table::eSynchronization2),
        .dynamicRendering = desc.device_features.required(rendering_features_table::eDynamicRender),
        .maintenance4     = desc.device_features.required(rendering_features_table::eMeshShading)};

    if (desc.device_features.required(rendering_features_table::eMeshShading))
    {
        VkPhysicalDeviceMeshShaderFeaturesEXT mesh_features = {
            .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
            .taskShader = true,
            .meshShader = true};
        vk13_features.pNext = &mesh_features;
    }

    VkPhysicalDeviceVulkan12Features vk12_features {
        .sType                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext                   = &vk13_features,
        .storageBuffer8BitAccess = desc.device_features.required(rendering_features_table::eMeshShading)};

    VkPhysicalDeviceVulkan11Features vk11_features {
        .sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext                = &vk12_features,
        .shaderDrawParameters = desc.device_features.required(rendering_features_table::eDrawIndirect)};

    VkPhysicalDeviceFeatures2 device_features {
        .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext    = &vk11_features,
        .features = {
                     .multiDrawIndirect = desc.device_features.required(rendering_features_table::eDrawIndirect),
                     }
    };

    VkDeviceCreateInfo device_create_info {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &device_features,
        .queueCreateInfoCount    = static_cast<u32>(queue_create_info.size()),
        .pQueueCreateInfos       = queue_create_info.data(),
        .enabledExtensionCount   = static_cast<u32>(context.enabled_device_extensions.size()),
        .ppEnabledExtensionNames = context.enabled_device_extensions.data(),
        .pEnabledFeatures        = nullptr,
    };

    VK_RETURN_ON_FAIL(vkCreateDevice(context.physical_device, &device_create_info, nullptr, &context.device));
    VK_RETURN_ON_FAIL(
        create_vma_allocator(context.instance, context.device, context.physical_device, &context.allocator));

    choose_queue_families(context.physical_device,
                          context.surface,
                          context.queues[queue_kind::eGfx].family,
                          context.queues[queue_kind::eCompute].family,
                          context.queues[queue_kind::eTransfer].family,
                          context.queues[queue_kind::ePresent].family);

    auto populate_queue_data = [&](const queue_kind_t idx)
    {
        vkGetDeviceQueue(context.device, context.queues[idx].family, 0, &context.queues[idx].queue);
    };

    for (u32 i = 0; i < queue_kind::COUNT; i++)
    {
        populate_queue_data(i);
    }

    return context;
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D requested)
{
    return {.width  = std::clamp(requested.width, caps.minImageExtent.width, caps.maxImageExtent.width),
            .height = std::clamp(requested.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
}

VkPresentModeKHR choose_present_mode(VkPhysicalDevice device, VkSurfaceKHR surface, bool vsync)
{
    ZoneScoped;
    if (vsync)
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    u32 present_modes = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes, nullptr);

    std::vector<VkPresentModeKHR> available_present_modes(present_modes);
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

VkFormat choose_depth_format(VkPhysicalDevice device)
{
    ZoneScoped;
    constexpr VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    constexpr VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (auto candidate : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device, candidate, &props);

        if ((props.optimalTilingFeatures & features) == features)
        {
            return candidate;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

bool format_has_stencil_component(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkSurfaceFormatKHR choose_swapchain_format(VkPhysicalDevice device, VkSurfaceKHR surface, VkFormat format)
{
    ZoneScoped;
    u32 surface_formats = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats, nullptr);

    std::vector<VkSurfaceFormatKHR> available_formats(surface_formats);
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

void render::destroy_swapchain(const context& vk_context, swapchain& swapchain)
{
    ZoneScoped;
    VK_DO_IF_NOT_NULL(vk_context.device, vkDeviceWaitIdle(vk_context.device));

    if (swapchain.vk_swapchain != VK_NULL_HANDLE)
    {
        for (auto& img : swapchain.images)
        {
            vkDestroyImageView(vk_context.device, img.depth_image_view, nullptr);
            destroy_image(vk_context.allocator, img.depth_image);

            vkDestroyImageView(vk_context.device, img.image_view, nullptr);
            vkDestroySemaphore(vk_context.device, img.release_semaphore, nullptr);
        }

        vkDestroySwapchainKHR(vk_context.device, swapchain.vk_swapchain, nullptr);
    }

    swapchain.vk_swapchain = VK_NULL_HANDLE;
    swapchain.images.clear();
}

result<swapchain> render::create_swapchain(const context& vk_context, VkFormat format, ivec2 size, u32 frames_in_flight,
                                           bool vsync, VkSwapchainKHR old_swapchain)
{
    ZoneScoped;
    swapchain sc_data;
    sc_data.surface_format = choose_swapchain_format(vk_context.physical_device, vk_context.surface, format);

    const std::unordered_set unique_queues {vk_context.queues[queue_kind::eGfx].family,
                                            vk_context.queues[queue_kind::ePresent].family};
    const std::vector<u32> unique_queues_as_vec {unique_queues.begin(), unique_queues.end()};

    const bool image_sharing_supported = unique_queues.size() > 1;

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context.physical_device, vk_context.surface, &capabilities);

    const VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext            = nullptr,
        .flags            = 0,
        .surface          = vk_context.surface,
        .minImageCount    = std::clamp(frames_in_flight,
                                    capabilities.minImageCount,
                                    capabilities.maxImageCount > 0 ? capabilities.maxImageCount : frames_in_flight),
        .imageFormat      = sc_data.surface_format.format,
        .imageColorSpace  = sc_data.surface_format.colorSpace,
        .imageExtent      = choose_extent(capabilities, {static_cast<u32>(size.x), static_cast<u32>(size.y)}),
        .imageArrayLayers = 1,
        .imageUsage =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode      = image_sharing_supported ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = image_sharing_supported ? static_cast<u32>(unique_queues_as_vec.size()) : 0,
        .pQueueFamilyIndices   = image_sharing_supported ? unique_queues_as_vec.data() : nullptr,
        .preTransform          = capabilities.currentTransform,
        .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode           = choose_present_mode(vk_context.physical_device, vk_context.surface, vsync),
        .clipped               = VK_TRUE,
        .oldSwapchain          = old_swapchain,
    };

    VK_RETURN_ON_FAIL(vkCreateSwapchainKHR(vk_context.device, &swapchain_create_info, nullptr, &sc_data.vk_swapchain));

    u32 img_count = 0;
    vkGetSwapchainImagesKHR(vk_context.device, sc_data.vk_swapchain, &img_count, nullptr);

    std::vector<VkImage> imgs(img_count);
    vkGetSwapchainImagesKHR(vk_context.device, sc_data.vk_swapchain, &img_count, imgs.data());

    const auto depth_format = sc_data.depth_format = choose_depth_format(vk_context.physical_device);
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

        swapchain_image sc_image {};
        sc_image.image = imgs[i];

        VK_RETURN_ON_FAIL(vkCreateImageView(vk_context.device, &image_view_create_info, nullptr, &sc_image.image_view));

        const VkSemaphoreCreateInfo semaphore_create_info {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_ASSERT_ON_FAIL(
            vkCreateSemaphore(vk_context.device, &semaphore_create_info, nullptr, &sc_image.release_semaphore));

        const VkImageCreateInfo image_create_info {
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = sc_data.depth_format,
            .extent        = {static_cast<u32>(size.x), static_cast<u32>(size.y), 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        const auto depth_img = create_image(image_create_info, vk_context.allocator);
        if (!depth_img)
        {
            return depth_img.message;
        }

        sc_image.depth_image = *depth_img;

        constexpr auto kDAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        const auto depth_img_view =
            create_image_view(vk_context.device, sc_image.depth_image.image, depth_format, kDAspect);
        if (!depth_img_view)
        {
            return depth_img.message;
        }

        sc_image.depth_image_view = *depth_img_view;
        sc_data.images.emplace_back(sc_image);
    }

    return sc_data;
}

void render::destroy_context(context& ctx)
{
    ZoneScoped;
    VK_DO_IF_NOT_NULL(ctx.device, vkDeviceWaitIdle(ctx.device));

    VK_DO_IF_NOT_NULL(ctx.allocator, vmaDestroyAllocator(ctx.allocator));
    VK_DO_IF_NOT_NULL(ctx.surface, vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr));
    VK_DO_IF_NOT_NULL(ctx.debug_messenger, vkDestroyDebugUtilsMessengerEXT(ctx.instance, ctx.debug_messenger, nullptr));

    VK_DO_IF_NOT_NULL(ctx.device, vkDestroyDevice(ctx.device, nullptr));
    VK_DO_IF_NOT_NULL(ctx.instance, vkDestroyInstance(ctx.instance, nullptr));
}

result<context> render::create_context(const window& window, const instance_desc& instance_desc)
{
    ZoneScoped;
    auto r_created_context = create_vk_context(window, instance_desc);
    if (!r_created_context)
    {
        return {r_created_context.message};
    }

    return *r_created_context;
}
