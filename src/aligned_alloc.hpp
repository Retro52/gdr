#pragma once

#include <types.hpp>

void free_aligned(void* ptr);
void* alloc_aligned(u64 size, u8 alignment);
void* realloc_aligned(void* memory, u64 size, u8 alignment);
