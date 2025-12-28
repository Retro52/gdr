#pragma once
#include <types.hpp>

#include <Tracy/Tracy.hpp>

#include <memory>

struct bytes
{
public:
    explicit bytes(u64 size)
        : m_size(size)
        , m_memory(std::make_unique<u8[]>(size))
    {
    }

    bytes(u64 size, const void* data)
        : bytes(size)
    {
        ZoneScoped;
        std::copy_n(static_cast<const u8*>(data), size, m_memory.get());
    }

    void* data() noexcept
    {
        return m_memory.get();
    }

    const void* data() const noexcept
    {
        return m_memory.get();
    }

    const void* begin() const noexcept
    {
        return m_memory.get();
    }

    const void* end() const noexcept
    {
        return m_memory.get() + m_size;
    }

    template<typename T>
    T* get() noexcept
        requires(std::is_integral_v<T> || std::is_void_v<T>)
    {
        return reinterpret_cast<T*>(m_memory.get());
    }

    template<typename T>
    const T* get() const noexcept
        requires(std::is_integral_v<T> || std::is_void_v<T>)
    {
        return reinterpret_cast<T*>(m_memory.get());
    }

    template<typename T>
    T& operator[](const u64 index) noexcept
    {
        return get<T>()[index];
    }

    template<typename T>
    const T& operator[](const u64 index) const noexcept
    {
        return get<T>()[index];
    }

    u64 size() const noexcept
    {
        return m_size;
    }

    bool empty() const noexcept
    {
        return m_size == 0;
    }

    operator bool() const noexcept
    {
        return !empty();
    }

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    u64 length() const noexcept
    {
        return size() / sizeof(T);
    }

private:
    u64 m_size;
    std::unique_ptr<u8[]> m_memory;
};
