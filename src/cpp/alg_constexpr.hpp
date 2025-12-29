#pragma once

#include <types.hpp>

namespace cpp
{
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

        return static_cast<const std::uint8_t>(*s1) - static_cast<const std::uint8_t>(*s2);
    }

    constexpr bool cx_streq(const char* s1, const char* s2)
    {
        return cx_strcmp(s1, s2) == 0;
    }

    constexpr void* cx_memset(void* dest, const int value, const u64 bytes)
    {
        auto* ptr = static_cast<std::uint8_t*>(dest);
        for (u64 i = 0; i < bytes; ++i)
        {
            ptr[i] = static_cast<std::uint8_t>(value);
        }

        return dest;
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
