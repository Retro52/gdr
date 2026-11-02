#pragma once

#include <core/alg_constexpr.hpp>
#include <core/hash/hashed_string.hpp>
#include <janitor.hpp>

#define SCOPED_GPU_PASS(RHI, CMD, CTX, NAME)                                                                      \
    const auto EXPR_CONCAT(_gpu_pass_name_hash_, __LINE__) = core::hash::crc32(NAME, core::cx_strlen(NAME));      \
    RHI.cmd_debug_label_begin(                                                                                    \
        CMD,                                                                                                      \
        CTX,                                                                                                      \
        {                                                                                                         \
            .label = NAME,                                                                                        \
            .color = vec4(static_cast<float>(EXPR_CONCAT(_gpu_pass_name_hash_, __LINE__) >> 0 & 0xFF) / 255.0F,   \
                          static_cast<float>(EXPR_CONCAT(_gpu_pass_name_hash_, __LINE__) >> 8 & 0xFF) / 255.0F,   \
                          static_cast<float>(EXPR_CONCAT(_gpu_pass_name_hash_, __LINE__) >> 16 & 0xFF) / 255.0F,  \
                          static_cast<float>(EXPR_CONCAT(_gpu_pass_name_hash_, __LINE__) >> 24 & 0xFF) / 255.0F), \
        });                                                                                                       \
    SUMMON_JANITOR(RHI.cmd_debug_label_end(CMD, CTX));
