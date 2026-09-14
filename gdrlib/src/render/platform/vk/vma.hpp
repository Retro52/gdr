#pragma once

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include <volk.h>

#if !defined(VULKAN_H_)
#define VULKAN_H_
#endif

#include <vk_mem_alloc.h>
