#pragma once

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <pod_types.hpp>
#include <tracy/Tracy.hpp>

#include <type_traits>
#include <utility>

namespace cpp
{
    template<typename T>
    class heap_array
    {
    private:
        T* m_data {nullptr};
        T* m_end {nullptr};
        T* m_capacity_end {nullptr};

    public:
        heap_array() = default;

        explicit heap_array(const u64 size)
        {
            ZoneScoped;
            resize(size);
            assert2(m_data != nullptr);
        }

        heap_array(const heap_array& other)
            : m_data {other.size() ? alloc(other.size()) : nullptr}
            , m_end {m_data + other.size()}
            , m_capacity_end {m_end}
        {
            ZoneScoped;
            heap_array::fast_copy_n(m_data, other.m_data, other.size());
        }

        heap_array(heap_array&& other) noexcept
            : m_data {other.m_data}
            , m_end {other.m_end}
            , m_capacity_end {other.m_capacity_end}
        {
            ZoneScoped;
            other.m_data         = nullptr;
            other.m_end          = nullptr;
            other.m_capacity_end = nullptr;
        }

        heap_array& operator=(const heap_array& other)
        {
            ZoneScoped;
            if (this != &other)
            {
                heap_array tmp(other);
                std::swap(this->m_data, tmp.m_data);
                std::swap(this->m_end, tmp.m_end);
                std::swap(this->m_capacity_end, tmp.m_capacity_end);
            }

            return *this;
        }

        heap_array& operator=(heap_array&& other) noexcept
        {
            ZoneScoped;
            if (this != &other)
            {
                destroy(m_data, m_end);
                dealloc(m_data);

                m_data         = other.m_data;
                m_end          = other.m_end;
                m_capacity_end = other.m_capacity_end;

                other.m_data         = nullptr;
                other.m_end          = nullptr;
                other.m_capacity_end = nullptr;
            }
            return *this;
        }

        ~heap_array()
        {
            destroy(m_data, m_end);
            dealloc(m_data);
        }

        [[nodiscard]] T* data() { return m_data; }

        [[nodiscard]] const T* data() const { return m_data; }

        [[nodiscard]] T* begin() { return m_data; }

        [[nodiscard]] const T* begin() const { return m_data; }

        [[nodiscard]] T* end() { return m_end; }

        [[nodiscard]] const T* end() const { return m_end; }

        [[nodiscard]] u64 size() const { return static_cast<u64>(m_end - m_data); }

        [[nodiscard]] u64 capacity() const { return static_cast<u64>(m_capacity_end - m_data); }

        [[nodiscard]] bool empty() const { return m_end == m_data; }

        T& operator[](u64 i)
        {
            assert2(i < size());
            return m_data[i];
        }

        const T& operator[](u64 i) const
        {
            assert2(i < size());
            return m_data[i];
        }

        void push_back(const T& value)
        {
            ZoneScoped;
            if (m_end >= m_capacity_end)
            {
                grow(get_next_capacity());
            }

            new (m_end++) T(value);
        }

        void push_back(T&& value)
        {
            ZoneScoped;
            if (m_end >= m_capacity_end)
            {
                grow(get_next_capacity());
            }

            new (m_end++) T(std::move(value));
        }

        template<typename... Args>
        T& emplace_back(Args&&... args)
        {
            ZoneScoped;
            if (m_end >= m_capacity_end)
            {
                grow(get_next_capacity());
            }

            new (m_end) T(std::forward<Args>(args)...);
            return *m_end++;
        }

        void append(heap_array&& other)
        {
            ZoneScoped;
            append(other.data(), other.size());
        }

        void append(const heap_array& other)
        {
            ZoneScoped;
            append(other.data(), other.size());
        }

        void append(const T* data, const u64 count)
        {
            ZoneScoped;
            if (m_end + count > m_capacity_end)
            {
                grow(cpp::max(size() + count, get_next_capacity()));
            }

            heap_array::fast_copy_n(m_end, data, count);
            m_end += count;
        }

        void reserve(const u64 new_capacity)
        {
            ZoneScoped;
            if (new_capacity > capacity())
            {
                grow(new_capacity);
            }
        }

        void resize(const u64 new_size)
        {
            ZoneScoped;
            if (new_size <= size())
            {
                destroy(m_data + new_size, m_end);
                m_end = m_data + new_size;
            }
            else if (new_size <= capacity())
            {
                init(m_end, m_data + new_size);
                m_end = m_data + new_size;
            }
            else
            {
                auto* data_tmp = alloc(new_size);

                heap_array::fast_move_n(data_tmp, m_data, size());

                destroy(m_data, m_end);
                dealloc(m_data);

                T* old_end_offset = data_tmp + size();

                m_data         = data_tmp;
                m_end          = data_tmp + new_size;
                m_capacity_end = data_tmp + new_size;

                init(old_end_offset, m_end);
            }
        }

        void clear()
        {
            ZoneScoped;

            destroy(m_data, m_end);
            m_end = m_data;
        }

    private:
        void init(T* start, T* const end)
        {
            assert2(start <= m_capacity_end && end <= m_capacity_end && start <= end);

            if constexpr (std::is_trivially_default_constructible_v<T>)
            {
                std::memset(start, 0, static_cast<u64>(end - start) * sizeof(T));
            }
            else
            {
                while (start < end)
                {
                    new (start++) T();
                }
            }
        }

        void destroy(T* start, T* const end)
        {
            assert2(start <= m_capacity_end && end <= m_capacity_end && start <= end);

            while (start < end)
            {
                (start++)->~T();
            }
        }

        void grow(const u64 new_capacity)
        {
            ZoneScoped;
            assert2(new_capacity > capacity());

            const u64 old_size = size();
            auto* data_tmp     = alloc(new_capacity);

            heap_array::fast_move_n(data_tmp, m_data, old_size);

            destroy(m_data, m_end);
            dealloc(m_data);

            m_data         = data_tmp;
            m_end          = data_tmp + old_size;
            m_capacity_end = data_tmp + new_capacity;
        }

        [[nodiscard]] u64 get_next_capacity() const noexcept
        {
            const u64 cap = capacity();
            return cap > 0 ? cap << 1 : 8;
        }

        static T* alloc(const u64 size)
        {
            return static_cast<T*>(::operator new(sizeof(T) * size, std::align_val_t {alignof(T)}));
        }

        static void dealloc(T* ptr) { ::operator delete(ptr, std::align_val_t {alignof(T)}); }

        template<typename U>
        constexpr static void fast_copy_n(U* dest, const U* src, u64 count)
        {
            if constexpr (std::is_trivially_copyable_v<U>)
            {
                cpp::cx_copy_n(dest, src, count);
            }
            else
            {
                cpp::cx_uninitialized_copy_n(dest, src, count);
            }
        }

        template<typename U>
        constexpr static void fast_move_n(U* dest, U* src, u64 count)
        {
            if constexpr (std::is_trivially_copyable_v<U>)
            {
                cpp::cx_copy_n(dest, src, count);
            }
            else
            {
                cpp::cx_uninitialized_move_n(dest, src, count);
            }
        }
    };
}
