#include <job/aligned_alloc.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>
#include <cstring>

namespace
{
    u64 align(const u64 addr, const u8 alignment)
    {
        const u64 mask = alignment - 1;
        assert((alignment & mask) == 0);

        return (addr + mask) & ~mask;
    }

    template<typename T>
    T* align_ptr(T* ptr, const u8 alignment)
    {
        const auto addr = reinterpret_cast<u64>(ptr);
        return reinterpret_cast<T*>(align(addr, alignment));
    }
}

void* job::alloc_aligned(const u64 size, const u8 alignment)
{
    ZoneScoped;
    constexpr u64 header_size = sizeof(u64);
    const u64 total_bytes     = header_size + size + alignment;

    auto* p            = new u8[total_bytes];
    auto* p_header_off = p + header_size;
    u8* p_aligned      = align_ptr(p_header_off, alignment);

    if (p_header_off == p_aligned)
    {
        p_aligned += alignment;
    }

    // save diff to the original pointer
    const i64 diff = p_aligned - p;
    assert(diff >= 0 && diff < 0xFF);  // 0xFF = 255 = 2^8 - 1
    p_aligned[-1] = static_cast<u8>(diff & 0xFF);

    // save total allocation size
    auto* p_header_dst = reinterpret_cast<u64*>(p_aligned - 1 - header_size);
    p_header_dst[0]    = size;

    return p_aligned;
}

void* job::realloc_aligned(void* memory, const u64 size, const u8 alignment)
{
    ZoneScoped;
    if (!memory)
    {
        return nullptr;
    }

    constexpr u64 header_size = sizeof(u64);

    auto* new_memory        = alloc_aligned(size, alignment);
    const u64 original_size = reinterpret_cast<u64*>(static_cast<u8*>(memory) - header_size - 1)[0];

    std::memcpy(new_memory, memory, std::min(original_size, size));
    free_aligned(memory);

    return new_memory;
}

void job::free_aligned(void* ptr)
{
    ZoneScoped;
    if (ptr)
    {
        const auto* p_aligned = static_cast<u8*>(ptr);
        const i64 diff        = p_aligned[-1];

        const u8* p = p_aligned - diff;
        delete[] p;
    }
}
