#pragma once

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <pod_types.hpp>

namespace cpp
{
    template<typename T>
    class heap_array
    {
    private:
        u64 m_size {0};
        u64 m_capacity {0};
        T* m_data {nullptr};

    public:
        heap_array() = default;

        explicit heap_array(u64 size)
            : m_size {size}
            , m_capacity {size}
            , m_data(new T[size])
        {
            assert2(m_data != nullptr);
        }

        template<typename It>
        explicit heap_array(It begin, const It end)
        {
            while (begin != end)
            {
                push_back(*begin++);
            }
        }

        heap_array(const heap_array& other)
            : m_size {other.m_size}
            , m_capacity {other.m_size}
            , m_data {other.m_size ? new T[other.m_size] : nullptr}
        {
            cpp::cx_copy_n(m_data, other.m_data, m_size);
        }

        heap_array(heap_array&& other) noexcept
            : m_size {other.m_size}
            , m_capacity {other.m_capacity}
            , m_data {other.m_data}
        {
            other.m_size     = 0;
            other.m_capacity = 0;
            other.m_data     = nullptr;
        }

        heap_array& operator=(const heap_array& other)
        {
            if (this != &other)
            {
                delete[] m_data;

                m_size     = other.m_size;
                m_capacity = other.m_size;
                m_data     = other.m_size ? new T[other.m_size] : nullptr;

                cpp::cx_copy_n(m_data, other.m_data, m_size);
            }
            return *this;
        }

        heap_array& operator=(heap_array&& other) noexcept
        {
            if (this != &other)
            {
                delete[] m_data;

                m_size     = other.m_size;
                m_capacity = other.m_capacity;
                m_data     = other.m_data;

                other.m_size     = 0;
                other.m_capacity = 0;
                other.m_data     = nullptr;
            }
            return *this;
        }

        ~heap_array() { delete[] m_data; }

        T* data() { return m_data; }

        const T* data() const { return m_data; }

        T* begin() { return m_data; }

        const T* begin() const { return m_data; }

        T* end() { return m_data + m_size; }

        const T* end() const { return m_data + m_size; }

        u64 size() const { return m_size; }

        u64 capacity() const { return m_capacity; }

        bool empty() const { return m_size == 0; }

        T& operator[](u64 i)
        {
            assert2(i < m_size);
            return m_data[i];
        }

        const T& operator[](u64 i) const
        {
            assert2(i < m_size);
            return m_data[i];
        }

        void push_back(const T& value)
        {
            if (m_size >= m_capacity)
            {
                grow(cpp::max(m_capacity * 4 / 3, u64(8)));
            }

            m_data[m_size++] = value;
        }

        void push_back(T&& value)
        {
            if (m_size >= m_capacity)
            {
                grow(cpp::max(m_capacity * 4 / 3, static_cast<u64>(8)));
            }

            m_data[m_size++] = static_cast<T&&>(value);
        }

        template<typename... Args>
        T& emplace_back(Args&&... args)
        {
            if (m_size >= m_capacity)
            {
                grow(cpp::max(m_capacity * 4 / 3, static_cast<u64>(8)));
            }

            m_data[m_size].~T();
            new (&m_data[m_size]) T(static_cast<Args&&>(args)...);
            return m_data[m_size++];
        }

        void copy_into(const T* data, const u64 count)
        {
            if (m_size + count > m_capacity)
            {
                grow(cpp::max(m_size + count, m_capacity * 4 / 3));
            }

            cpp::cx_copy_n(m_data + m_size, data, count);
            m_size += count;
        }

    private:
        void grow(const u64 new_capacity)
        {
            assert2(new_capacity > m_capacity);

            m_capacity     = new_capacity;
            auto* data_tmp = new T[m_capacity];
            cpp::cx_copy_n(data_tmp, m_data, m_size);
            delete[] m_data;
            m_data = data_tmp;
        }
    };
}
