#include <log.hpp>

#define VMA_IMPLEMENTATION
#define VMA_DEBUG_LOG_FORMAT(format, ...) \
    do                                    \
    {                                     \
        LOG_DEBUG((format), __VA_ARGS__); \
    } while (false)

#include <vk_mem_alloc.h>
