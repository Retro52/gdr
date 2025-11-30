#pragma once

#include <SDL3/SDL_platform_defines.h>

#include <glm/glm.hpp>

#define DYE_ARR_SIZE(x) \
    ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))  // NOLINT(*-misplaced-array-index)

#define DYE_CONCAT_(a, b) a##b
#define DYE_CONCAT(a, b)  DYE_CONCAT_(a, b)

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = glm::float32_t;
using f64 = glm::float64_t;

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using ivec4 = glm::ivec4;

using uvec2 = glm::uvec2;
using uvec3 = glm::uvec3;
using uvec4 = glm::uvec4;

// type-erased pointer
using vptr = std::uintptr_t;

using bytes = std::vector<u8>;
