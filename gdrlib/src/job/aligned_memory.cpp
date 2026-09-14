#include <job/aligned_alloc.hpp>
#include <job/aligned_memory.hpp>

#include <cassert>
#include <tracy/Tracy.hpp>

using namespace job;

aligned_memory::aligned_memory(aligned_memory&& other) noexcept
    : m_ptr(other.m_ptr)
{
    ZoneScoped;
    other.m_ptr = nullptr;
}

aligned_memory& aligned_memory::operator=(aligned_memory&& other) noexcept
{
    ZoneScoped;
    if (this != &other)
    {
        ::free_aligned(m_ptr);
        m_ptr       = other.m_ptr;
        other.m_ptr = nullptr;
    }
    return *this;
}

aligned_memory::aligned_memory(u64 size, u8 alignment)
{
    ZoneScoped;
    if (alignment < alignof(void*) || (alignment & (alignment - 1)) != 0 || size % alignment > 0)
    {
        assert(false && "Alignment must be a power of two and at least sizeof(void*)");
    }

    // As of 2025 std::aligned_alloc is considered unimplementable for MSVC, see
    // https://learn.microsoft.com/en-us/cpp/overview/visual-cpp-language-conformance?view=msvc-170
    // MSVC provides the replacement in _aligned_malloc / _aligned_free, but to avoid conditional compilation, a
    // replacement function was created.
    m_ptr = ::alloc_aligned(size, alignment);
}

aligned_memory::~aligned_memory()
{
    ZoneScoped;
    ::free_aligned(m_ptr);
}

void* aligned_memory::get() const noexcept
{
    ZoneScoped;
    return m_ptr;
}

aligned_memory::operator void*() const noexcept
{
    ZoneScoped;
    return m_ptr;
}
