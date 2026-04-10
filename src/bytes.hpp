#pragma once

#include <assert2.hpp>
#include <pod_types.hpp>
#include <tracy/Tracy.hpp>

struct bytes
{
    explicit bytes(const u64 size)
        : m_size(size)
        , m_memory(new u8[size])
    {
    }

    bytes(const u64 size, const void* data)
        : bytes(size)
    {
        ZoneScoped;
        std::copy_n(static_cast<const u8*>(data), size, m_memory);
    }

    ~bytes() { delete[] m_memory; }

    bytes(const bytes& other)
        : bytes(other.size())
    {
        ZoneScoped;
        std::copy_n(other.get<u8>(), other.size(), m_memory);
    }

    bytes(bytes&& other) noexcept
        : m_size(other.m_size)
        , m_memory(other.m_memory)
    {
        ZoneScoped;

        other.m_size   = 0;
        other.m_memory = nullptr;
    }

    bytes& operator=(bytes&& other) noexcept
    {
        ZoneScoped;

        if (this == &other)
        {
            return *this;
        }

        delete[] m_memory;

        m_size   = other.m_size;
        m_memory = other.m_memory;

        other.m_memory = nullptr;
        other.m_size   = 0;

        return *this;
    }

    bytes& operator=(const bytes& other) noexcept
    {
        ZoneScoped;

        if (this == &other)
        {
            return *this;
        }

        delete[] m_memory;

        m_size   = other.m_size;
        m_memory = new u8[m_size];
        std::copy_n(other.get<u8>(), other.size(), m_memory);

        return *this;
    }

    [[nodiscard]] void* data() noexcept { return m_memory; }

    [[nodiscard]] const void* data() const noexcept { return m_memory; }

    [[nodiscard]] const void* begin() const noexcept { return m_memory; }

    [[nodiscard]] const void* end() const noexcept { return m_memory + m_size; }

    template<typename T>
    [[nodiscard]] T* get() noexcept
    {
        assert2(size() % sizeof(T) == 0);
        return reinterpret_cast<T*>(m_memory);
    }

    template<typename T>
    [[nodiscard]] const T* get() const noexcept
        requires(std::is_integral_v<T> || std::is_void_v<T>)
    {
        return reinterpret_cast<T*>(m_memory);
    }

    [[nodiscard]] u8& operator[](const u64 index) noexcept { return get<u8>()[index]; }

    [[nodiscard]] const u8& operator[](const u64 index) const noexcept { return get<u8>()[index]; }

    [[nodiscard]] u64 size() const noexcept { return m_size; }

    template<typename T>
    [[nodiscard]] u64 length() const noexcept
    {
        return size() / sizeof(T);
    }

    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

    operator bool() const noexcept { return !empty(); }

private:
    u64 m_size {0};
    u8* m_memory {nullptr};
};
