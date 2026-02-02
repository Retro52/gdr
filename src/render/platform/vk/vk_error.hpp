#pragma once

#include <volk.h>

#include <assert2.hpp>

#define VK_FAIL_HANDLE(EXPR, FAIL_OP)                                                              \
    switch (res)                                                                                   \
    {                                                                                              \
    case VK_ERROR_OUT_OF_HOST_MEMORY :                                                             \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_HOST_MEMORY)");                           \
        break;                                                                                     \
    case VK_ERROR_OUT_OF_DEVICE_MEMORY :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_DEVICE_MEMORY)");                         \
        break;                                                                                     \
    case VK_ERROR_INITIALIZATION_FAILED :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INITIALIZATION_FAILED)");                        \
        break;                                                                                     \
    case VK_ERROR_DEVICE_LOST :                                                                    \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_DEVICE_LOST)");                                  \
        break;                                                                                     \
    case VK_ERROR_MEMORY_MAP_FAILED :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_MEMORY_MAP_FAILED)");                            \
        break;                                                                                     \
    case VK_ERROR_LAYER_NOT_PRESENT :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_LAYER_NOT_PRESENT)");                            \
        break;                                                                                     \
    case VK_ERROR_EXTENSION_NOT_PRESENT :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_EXTENSION_NOT_PRESENT)");                        \
        break;                                                                                     \
    case VK_ERROR_FEATURE_NOT_PRESENT :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FEATURE_NOT_PRESENT)");                          \
        break;                                                                                     \
    case VK_ERROR_INCOMPATIBLE_DRIVER :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INCOMPATIBLE_DRIVER)");                          \
        break;                                                                                     \
    case VK_ERROR_TOO_MANY_OBJECTS :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_TOO_MANY_OBJECTS)");                             \
        break;                                                                                     \
    case VK_ERROR_FORMAT_NOT_SUPPORTED :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FORMAT_NOT_SUPPORTED)");                         \
        break;                                                                                     \
    case VK_ERROR_FRAGMENTED_POOL :                                                                \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FRAGMENTED_POOL)");                              \
        break;                                                                                     \
    case VK_ERROR_UNKNOWN :                                                                        \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_UNKNOWN)");                                      \
        break;                                                                                     \
    case VK_ERROR_OUT_OF_POOL_MEMORY :                                                             \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_POOL_MEMORY)");                           \
        break;                                                                                     \
    case VK_ERROR_INVALID_EXTERNAL_HANDLE :                                                        \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_EXTERNAL_HANDLE)");                      \
        break;                                                                                     \
    case VK_ERROR_FRAGMENTATION :                                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FRAGMENTATION)");                                \
        break;                                                                                     \
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS :                                                 \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)");               \
        break;                                                                                     \
    case VK_PIPELINE_COMPILE_REQUIRED :                                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_PIPELINE_COMPILE_REQUIRED)");                          \
        break;                                                                                     \
    case VK_ERROR_NOT_PERMITTED :                                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NOT_PERMITTED)");                                \
        break;                                                                                     \
    case VK_ERROR_SURFACE_LOST_KHR :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_SURFACE_LOST_KHR)");                             \
        break;                                                                                     \
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR)");                     \
        break;                                                                                     \
    case VK_SUBOPTIMAL_KHR :                                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_SUBOPTIMAL_KHR)");                                     \
        break;                                                                                     \
    case VK_ERROR_OUT_OF_DATE_KHR :                                                                \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_OUT_OF_DATE_KHR)");                              \
        break;                                                                                     \
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR)");                     \
        break;                                                                                     \
    case VK_ERROR_INVALID_SHADER_NV :                                                              \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_SHADER_NV)");                            \
        break;                                                                                     \
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR :                                                  \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)");                \
        break;                                                                                     \
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR :                                         \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)");       \
        break;                                                                                     \
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR :                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)");    \
        break;                                                                                     \
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR :                                         \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)");       \
        break;                                                                                     \
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR :                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)");        \
        break;                                                                                     \
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR :                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)");          \
        break;                                                                                     \
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT :                                   \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)"); \
        break;                                                                                     \
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT :                                            \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)");          \
        break;                                                                                     \
    case VK_THREAD_IDLE_KHR :                                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_THREAD_IDLE_KHR)");                                    \
        break;                                                                                     \
    case VK_THREAD_DONE_KHR :                                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_THREAD_DONE_KHR)");                                    \
        break;                                                                                     \
    case VK_OPERATION_DEFERRED_KHR :                                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_OPERATION_DEFERRED_KHR)");                             \
        break;                                                                                     \
    case VK_OPERATION_NOT_DEFERRED_KHR :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_OPERATION_NOT_DEFERRED_KHR)");                         \
        break;                                                                                     \
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR :                                               \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR)");             \
        break;                                                                                     \
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT :                                                      \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_COMPRESSION_EXHAUSTED_EXT)");                    \
        break;                                                                                     \
    case VK_INCOMPATIBLE_SHADER_BINARY_EXT :                                                       \
        FAIL_OP(#EXPR " failed. Reason: (VK_INCOMPATIBLE_SHADER_BINARY_EXT)");                     \
        break;                                                                                     \
    case VK_PIPELINE_BINARY_MISSING_KHR :                                                          \
        FAIL_OP(#EXPR " failed. Reason: (VK_PIPELINE_BINARY_MISSING_KHR)");                        \
        break;                                                                                     \
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR :                                                           \
        FAIL_OP(#EXPR " failed. Reason: (VK_ERROR_NOT_ENOUGH_SPACE_KHR)");                         \
        break;                                                                                     \
    default :                                                                                      \
        FAIL_OP(#EXPR " failed. Reason: Unknown VK error");                                        \
        break;                                                                                     \
    }

#if !defined(NDEBUG)
#define VK_ASSERT(MSG) assert2(false && (MSG))

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

