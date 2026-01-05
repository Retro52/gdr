#pragma once

#include <types.hpp>

#include <Tracy/Tracy.hpp>

#include <memory>

struct bytes
{
public:
    explicit bytes(const u64 size)
        : m_size(size)
        , m_memory(std::make_unique<u8[]>(size))
    {
    }

    bytes(const u64 size, const void* data)
        : bytes(size)
    {
        ZoneScoped;
        std::copy_n(static_cast<const u8*>(data), size, m_memory.get());
    }

    bytes(bytes&&)            = default;
    bytes& operator=(bytes&&) = default;

    bytes(const bytes& other)
        : bytes(other.size())
    {
        std::copy_n(other.get<u8>(), other.size(), m_memory.get());
    }

    bytes& operator=(const bytes& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_memory = std::make_unique<u8[]>(other.size());
        std::copy_n(other.get<u8>(), other.size(), m_memory.get());
        return *this;
    }

    [[nodiscard]] void* data() noexcept
    {
        return m_memory.get();
    }

    [[nodiscard]] const void* data() const noexcept
    {
        return m_memory.get();
    }

    [[nodiscard]] const void* begin() const noexcept
    {
        return m_memory.get();
    }

    [[nodiscard]] const void* end() const noexcept
    {
        return m_memory.get() + m_size;
    }

    template<typename T>
    [[nodiscard]] T* get() noexcept
    {
        assert(size() % sizeof(T) == 0);
        return reinterpret_cast<T*>(m_memory.get());
    }

    template<typename T>
    [[nodiscard]] const T* get() const noexcept
        requires(std::is_integral_v<T> || std::is_void_v<T>)
    {
        return reinterpret_cast<T*>(m_memory.get());
    }

    template<typename T>
    [[nodiscard]] T& operator[](const u64 index) noexcept
    {
        return get<T>()[index];
    }

    template<typename T>
    [[nodiscard]] const T& operator[](const u64 index) const noexcept
    {
        return get<T>()[index];
    }

    [[nodiscard]] u64 size() const noexcept
    {
        return m_size;
    }

    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    [[nodiscard]] u64 length() const noexcept
    {
        return size() / sizeof(T);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_size == 0;
    }

    operator bool() const noexcept
    {
        return !empty();
    }

private:
    u64 m_size;
    std::unique_ptr<u8[]> m_memory;
};
