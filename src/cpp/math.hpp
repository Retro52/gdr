#pragma once

#include <types.hpp>

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

namespace cpp
{
    template<typename Type>
    inline constexpr Type min(const Type& x, const Type& y) noexcept
    {
        return x < y ? x : y;
    }

    template<typename Type>
    inline constexpr Type max(const Type& x, const Type& y) noexcept
    {
        return y < x ? x : y;
    }

    template<typename Type>
    inline constexpr Type clamp(const Type& x, const Type& a, const Type& b) noexcept
    {
        return x < a ? a : b < x ? b : x;
    }

    template<typename Num>
    inline constexpr Num abs(Num x) noexcept
    {
        return x < 0 ? -x : x;
    }

    template<typename Num>
    inline constexpr Num signum(Num x) noexcept
    {
        return x < 0 ? -1 : 0 < x ? 1 : 0;
    }

    template<typename Int>
    inline constexpr Int extend_sign(Int x, unsigned w) noexcept
    {
        w = 8 * sizeof(Int) - w;
        return (x << w) >> w;
    }

    template<typename Int>
    inline constexpr Int swap_bits(Int x, unsigned i, unsigned j, unsigned n) noexcept
    {
        Int w = ((x >> i) ^ (x >> j)) & ((1 << n) - 1);
        return x ^ ((w << i) | (w << j));
    }

    inline constexpr u8 parity(u8 x) noexcept
    {
        x ^= x >> 4;

        return (0x6996 >> (x & 0x0F)) & 1;
    }

    inline constexpr u16 parity(u16 x) noexcept
    {
        x ^= x >> 8;
        x ^= x >> 4;

        return (0x6996 >> (x & 0x0F)) & 1;
    }

    inline constexpr u32 parity(u32 x) noexcept
    {
        x ^= x >> 16;
        x ^= x >> 8;
        x ^= x >> 4;

        return (0x6996 >> (x & 0x0F)) & 1;
    }

    inline constexpr u64 parity(u64 x) noexcept
    {
        x ^= x >> 32;
        x ^= x >> 16;
        x ^= x >> 8;
        x ^= x >> 4;

        return (0x6996 >> (x & 0x0F)) & 1;
    }

    template<typename Int>
    inline constexpr Int gcd(Int x, Int y) noexcept
    {
        Int z(0);

        // ensure non-negative values and possible user-defined type
        if (x < z)
        {
            x = z - x;
        }
        if (y < z)
        {
            y = z - y;
        }

        while (true)
        {
            // check if maximum reduction reached
            if (y == z)
            {
                return x;
            }
            x %= y;

            // symmetric case
            if (x == z)
            {
                return y;
            }
            y %= x;
        }
    }

    template<typename Int>
    inline constexpr Int lcm(Int x, Int y) noexcept
    {
        Int z(0);

        if (x == z || y == z)
        {
            return z;
        }
        x /= gcd(x, y);
        x *= y;

        // ensure non-negative value
        if (x < z)
        {
            x = z - x;
        }
        return x;
    }

    template<typename Num>
    inline constexpr Num pow(Num x, unsigned n) noexcept
    {
        Num p = (n % 2) ? x : 1;

        while (n >>= 1)
        {
            x = x * x;

            if (n % 2)
            {
                p = p * x;
            }
        }
        return p;
    }

    template<typename T>
    constexpr T saturated_add(T a, T b) noexcept
    {
        static_assert(std::is_integral_v<T>, "Type must be integral");

        if ((b > 0) && (a > std::numeric_limits<T>::max() - b))  // Overflow check
        {
            return std::numeric_limits<T>::max();
        }
        else if ((b < 0) && (a < std::numeric_limits<T>::min() - b))  // Underflow check
        {
            return std::numeric_limits<T>::min();
        }

        return a + b;
    }

    template<typename T>
    constexpr T saturated_subtract(T a, T b) noexcept
    {
        static_assert(std::is_integral_v<T>, "Type must be integral");

        if ((b < 0) && (a > std::numeric_limits<T>::max() + b))  // Overflow check
        {
            return std::numeric_limits<T>::max();
        }
        else if ((b > 0) && (a < std::numeric_limits<T>::min() + b))  // Underflow check
        {
            return std::numeric_limits<T>::min();
        }

        return a - b;
    }

    template<typename Int>
    inline constexpr bool is_pow2(Int x) noexcept
    {
        return x > 0 && (x & (x - 1)) == 0;
    }

    template<int Prec, typename Int>
    inline constexpr Int sqrti(Int x) noexcept
    {
        int bits = sizeof(Int) * 8;
        int prec = bits / 2 + Prec;

        // initialize accumulator, remainder, and test value
        Int a = 0;
        Int r = 0;
        Int e = 0;

        for (int i = 0; i < prec; i++)
        {
            r = (r << 2) + ((x >> (bits - 2)) & 3);
            x <<= 2;
            a <<= 1;
            e = (a << 1) + 1;

            if (r >= e)
            {
                r -= e;
                a += 1;
            }
        }
        return a;
    }

    inline constexpr double rad2deg(double rad) noexcept
    {
        return rad * 180.0 / std::numbers::pi;
    }

    inline constexpr double deg2rad(double deg) noexcept
    {
        return deg * std::numbers::pi / 180.0;
    }

    template<typename T>
    inline constexpr std::enable_if_t<!std::is_floating_point_v<T>, bool> is_equal(const T& a, const T& b) noexcept
    {
        return a == b;
    }

    template<typename T>
    inline constexpr std::enable_if_t<std::is_floating_point_v<T>, bool> is_equal(T a, T b) noexcept
    {
        T factor = max(static_cast<T>(1), max(std::fabs(a), std::fabs(b)));
        return std::fabs(a - b) <= std::numeric_limits<T>::epsilon() * factor;
    }

    template<typename T>
    inline constexpr std::enable_if_t<std::is_floating_point_v<T>, bool> is_close(T a, T b, u64 precision) noexcept
    {
        T epsilon = static_cast<T>(std::pow(10, -static_cast<int>(precision)));
        return std::fabs(a - b) <= epsilon;
    }

    template<typename T>
    inline constexpr T sign(T val) noexcept
    {
        return (T(0) < val) - (val < T(0));
    }

    template<typename T>
    inline constexpr bool in_range(T value, T lower, T upper) noexcept
    {
        return (value > lower || is_equal(value, lower)) && (value < upper || is_equal(value, upper));
    }
}
