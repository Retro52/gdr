#pragma once

#define COUNT_OF(x) \
    ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))  // NOLINT(*-misplaced-array-index)

// controls whether certain "core" structs can have overloads with STL types
#define ENABLE_STL 0

#define EXPR_CONCAT_(a, b) a##b
#define EXPR_CONCAT(a, b)  EXPR_CONCAT_(a, b)

#if !defined(NDEBUG)
#define DEBUG_ONLY(EXPR) EXPR
#define NDEBUG_ONLY(EXPR)
#else
#define DEBUG_ONLY(EXPR)
#define NDEBUG_ONLY(EXPR) EXPR
#endif

#if TRACY_ENABLE
#define TRACY_ONLY(EXPR) EXPR
#else
#define TRACY_ONLY(EXPR)
#endif

#include <stdint.h>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;
