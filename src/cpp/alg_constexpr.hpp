#pragma once

#include <types.hpp>
#include <assert2.hpp>

namespace cpp
{
    template<typename T, typename U>
    constexpr bool cx_implies(T required, U supported) noexcept
        requires(std::is_integral_v<T> && std::is_integral_v<U>)
    {
        return !required || supported;
    }

    constexpr u32 cx_get_enum_bit_count(u64 enum_count_value) noexcept
    {
        u32 count      = 0;
        auto prev_flag = enum_count_value - 1;

        while (prev_flag > 0)
        {
            assert2(prev_flag == 1 || prev_flag % 2 == 0);

            ++count;
            prev_flag = prev_flag >> 1;
        }

        return count;
    }

    template<class It, class V>
    constexpr bool cx_contains(It first, const It end, const V& val)
    {
        while (first != end)
        {
            if (*first++ == val)
            {
                return true;
            }
        }

        return false;
    }

    constexpr u64 cx_strlen(const char* str)
    {
        auto result = static_cast<u64>(0);
        while (*(str++) != '\0')
        {
            ++result;
        }

        return result;
    }

    constexpr int cx_strcmp(const char* s1, const char* s2)
    {
        while (*s1 && (*s1 == *s2))
        {
            s1++;
            s2++;
        }

        return static_cast<const u8>(*s1) - static_cast<const u8>(*s2);
    }

    constexpr bool cx_streq(const char* s1, const char* s2)
    {
        return cx_strcmp(s1, s2) == 0;
    }

    constexpr void* cx_memcpy(void* dest, const void* src, const u64 bytes)
    {
        auto* dst_ptr    = static_cast<u8*>(dest);
        auto* source_ptr = static_cast<const u8*>(src);

        for (u64 i = 0; i < bytes; ++i)
        {
            dst_ptr[i] = source_ptr[i];
        }

        return dest;
    }

    template<typename It, typename T>
    constexpr void cx_fill(It&& begin, const It& end, T&& value)
    {
        while (begin != end)
        {
            *begin++ = value;
        }
    }

    template<typename T>
    constexpr T* cx_copy_n(T* dest, const T* src, u64 count)
    {
        for (u64 i = 0; i < count; ++i)
        {
            dest[i] = src[i];
        }
        return dest;
    }
}
