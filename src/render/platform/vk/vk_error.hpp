#pragma once

#include <volk.h>

#define VK_FAIL_HANDLE(EXPR, FAIL_OP)                                                              \
    switch (res)                                                                                   \
    {                                                                                              \
    case VK_ERROR_OUT_OF_HOST_MEMORY :                                                             \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_HOST_MEMORY)");                           \
    case VK_ERROR_OUT_OF_DEVICE_MEMORY :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_DEVICE_MEMORY)");                         \
    case VK_ERROR_INITIALIZATION_FAILED :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INITIALIZATION_FAILED)");                        \
    case VK_ERROR_DEVICE_LOST :                                                                    \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_DEVICE_LOST)");                                  \
    case VK_ERROR_MEMORY_MAP_FAILED :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_MEMORY_MAP_FAILED)");                            \
    case VK_ERROR_LAYER_NOT_PRESENT :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_LAYER_NOT_PRESENT)");                            \
    case VK_ERROR_EXTENSION_NOT_PRESENT :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_EXTENSION_NOT_PRESENT)");                        \
    case VK_ERROR_FEATURE_NOT_PRESENT :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FEATURE_NOT_PRESENT)");                          \
    case VK_ERROR_INCOMPATIBLE_DRIVER :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INCOMPATIBLE_DRIVER)");                          \
    case VK_ERROR_TOO_MANY_OBJECTS :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_TOO_MANY_OBJECTS)");                             \
    case VK_ERROR_FORMAT_NOT_SUPPORTED :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FORMAT_NOT_SUPPORTED)");                         \
    case VK_ERROR_FRAGMENTED_POOL :                                                                \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FRAGMENTED_POOL)");                              \
    case VK_ERROR_UNKNOWN :                                                                        \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_UNKNOWN)");                                      \
    case VK_ERROR_VALIDATION_FAILED :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VALIDATION_FAILED)");                            \
    case VK_ERROR_OUT_OF_POOL_MEMORY :                                                             \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_POOL_MEMORY)");                           \
    case VK_ERROR_INVALID_EXTERNAL_HANDLE :                                                        \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_EXTERNAL_HANDLE)");                      \
    case VK_ERROR_FRAGMENTATION :                                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FRAGMENTATION)");                                \
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS :                                                 \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)");               \
    case VK_PIPELINE_COMPILE_REQUIRED :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_PIPELINE_COMPILE_REQUIRED)");                          \
    case VK_ERROR_NOT_PERMITTED :                                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NOT_PERMITTED)");                                \
    case VK_ERROR_SURFACE_LOST_KHR :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_SURFACE_LOST_KHR)");                             \
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)");                     \
    case VK_SUBOPTIMAL_KHR :                                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_SUBOPTIMAL_KHR)");                                     \
    case VK_ERROR_OUT_OF_DATE_KHR :                                                                \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_DATE_KHR)");                              \
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)");                     \
    case VK_ERROR_INVALID_SHADER_NV :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_SHADER_NV)");                            \
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR :                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)");                \
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR :                                         \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)");       \
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR :                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)");    \
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR :                                         \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)");       \
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR :                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)");        \
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR :                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)");          \
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT :                                   \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)"); \
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT :                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)");          \
    case VK_THREAD_IDLE_KHR :                                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_THREAD_IDLE_KHR)");                                    \
    case VK_THREAD_DONE_KHR :                                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_THREAD_DONE_KHR)");                                    \
    case VK_OPERATION_DEFERRED_KHR :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_OPERATION_DEFERRED_KHR)");                             \
    case VK_OPERATION_NOT_DEFERRED_KHR :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_OPERATION_NOT_DEFERRED_KHR)");                         \
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR :                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR)");             \
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT :                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_COMPRESSION_EXHAUSTED_EXT)");                    \
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_INCOMPATIBLE_SHADER_BINARY_EXT)");                     \
    case VK_PIPELINE_BINARY_MISSING_KHR :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_PIPELINE_BINARY_MISSING_KHR)");                        \
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NOT_ENOUGH_SPACE_KHR)");                         \
    default :                                                                                      \
        FAIL_OP(#EXPR " failed. Reason: Unknown VK error");                                        \
    }

#if !defined(NDEBUG)
#define VK_ASSERT(MSG) assert(false && (MSG))

#define VK_ASSERT_ON_FAIL(EXPR)                   \
    if (const auto res = EXPR; res != VK_SUCCESS) \
    {                                             \
        VK_FAIL_HANDLE(EXPR, VK_ASSERT)           \
    }
#else
#define VK_ASSERT(MSG)
#define VK_ASSERT_ON_FAIL(EXPR) EXPR;
#endif

#define VK_RETURN_ON_FAIL(EXPR)                   \
    if (const auto res = EXPR; res != VK_SUCCESS) \
    {                                             \
        VK_FAIL_HANDLE(EXPR, return)              \
    }

