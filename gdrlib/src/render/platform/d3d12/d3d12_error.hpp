#pragma once

#include <assert2.hpp>
#include <render/platform/d3d12/d3d12.hpp>

#define D3D12_FAIL_HANDLE(EXPR, FAIL_OP) FAIL_OP(#EXPR " failed");

#if !defined(NDEBUG)
#define D3D12_ASSERT(MSG) assert2(false && (MSG))

#define D3D12_ASSERT_ON_FAIL(EXPR)            \
    if (const auto res = EXPR; FAILED(res))   \
    {                                         \
        D3D12_FAIL_HANDLE(EXPR, D3D12_ASSERT) \
    }
#else
#define D3D12_ASSERT(MSG)
#define D3D12_ASSERT_ON_FAIL(EXPR) EXPR;
#endif

#define D3D12_RETURN_ON_FAIL(EXPR)            \
    if (const auto res = EXPR; FAILED(res))   \
    {                                         \
        D3D12_FAIL_HANDLE(EXPR, return error) \
    }
