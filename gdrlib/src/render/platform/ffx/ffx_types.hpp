#pragma once

#include <ffx_api/ffx_api.hpp>
#include <ffx_api/ffx_api_types.h>

#define FFX_FAIL_HANDLE(EXPR, FAIL_OP)                            \
    switch (res)                                                  \
    {                                                             \
    case ffx::ReturnCode::ErrorUnknownDesctype :                  \
        FAIL_OP(#EXPR " failed. Reason: (ErrorUnknownDesctype)"); \
    case ffx::ReturnCode::ErrorRuntimeError :                     \
        FAIL_OP(#EXPR " failed. Reason: (ErrorRuntimeError)");    \
    case ffx::ReturnCode::ErrorNoProvider :                       \
        FAIL_OP(#EXPR " failed. Reason: (ErrorNoProvider)");      \
    case ffx::ReturnCode::ErrorMemory :                           \
        FAIL_OP(#EXPR " failed. Reason: (ErrorMemory)");          \
    case ffx::ReturnCode::ErrorParameter :                        \
        FAIL_OP(#EXPR " failed. Reason: (ErrorParameter)");       \
    default :                                                     \
    case ffx::ReturnCode::Error :                                 \
        FAIL_OP(#EXPR " failed. Reason: (Error)");                \
    }

#if !defined(NDEBUG)
#define FFX_ASSERT(MSG) assert2(false && (MSG))

#define FFX_ASSERT_ON_FAIL(EXPR)                                        \
    if (const auto res = EXPR; res != ffx::ReturnCode::Ok) [[unlikely]] \
    {                                                                   \
        FFX_FAIL_HANDLE(EXPR, FFX_ASSERT)                               \
    }
#else
#define FFX_ASSERT(MSG)
#define FFX_ASSERT_ON_FAIL(EXPR) EXPR;
#endif

#define FFX_RETURN_ON_FAIL(EXPR)                                        \
    if (const auto res = EXPR; res != ffx::ReturnCode::Ok) [[unlikely]] \
    {                                                                   \
        FFX_FAIL_HANDLE(EXPR, return error)                             \
    }

namespace platform
{
    constexpr FfxApiResource ffx_null_resource = {.resource = nullptr};

    struct ffx_upscale_resources
    {
        FfxApiResource color          = ffx_null_resource;
        FfxApiResource depth          = ffx_null_resource;
        FfxApiResource motion_vectors = ffx_null_resource;

        FfxApiResource output = ffx_null_resource;

        FfxApiResource reactive                     = ffx_null_resource;
        FfxApiResource transparency_and_composition = ffx_null_resource;
    };
}

