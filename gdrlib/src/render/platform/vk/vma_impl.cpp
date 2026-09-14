#include <cpp/containers/stack_string.hpp>
#include <log.hpp>

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                            \
    do                                                                               \
    {                                                                                \
        const auto log = cpp::big_stack_string::make_formatted(format, __VA_ARGS__); \
        LOG_TRACE_L3("{}", log.c_str());                                             \
    } while (false)

#include <vk_mem_alloc.h>
