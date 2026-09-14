#pragma once

#include <assert2.hpp>
#include <pod_types.hpp>

namespace cpp
{
    template<typename T, u64 N>
    struct tagged_int
    {
    private:
        T m_value = 0;

    private:
        constexpr static T kIdentity  = static_cast<T>(1);
        constexpr static T kFlagsMask = (kIdentity << N) - 1;

        constexpr static u8 kTotalBits = sizeof(T) * 8;
        constexpr static u8 kValueBits = sizeof(T) * 8 - N;

        constexpr static T kMaxValue = kIdentity << static_cast<T>(kValueBits) - 1;

#if 0
        // I think it can support signed integers as well, but then I'd have to overcomplicate the
        // set assertion & probably some other stuff
        // it would also probably mean I'd need to include type_traits and I don't really want to do that
        static_assert((-1 >> 1) == -1, "tagged_int requires arithmetic right shift");
#endif
        static_assert(N < kTotalBits, "tagged_int requires N to be smaller than the size of arithmetic type");

    public:
        constexpr tagged_int() = default;

        constexpr tagged_int(T value) noexcept { set(value); }

        constexpr bool get_flag(u32 index) const noexcept
        {
            assert2m(index < N, "flag index out of range");
            return m_value & (kIdentity << index);
        }

        constexpr void set_flag(u32 index, bool value) noexcept
        {
            assert2m(index < N, "flag index out of range");
            m_value = value ? m_value | (kIdentity << index) : m_value & ~(kIdentity << index);
        }

        constexpr T value() const noexcept { return m_value >> N; }

        constexpr void set(T value) noexcept
        {
            assert2m(value <= kMaxValue, "value exceeds representable range");
            m_value = (value << N) | (m_value & kFlagsMask);
        }

        constexpr operator T() const noexcept { return value(); }

        constexpr T operator+(T other) const noexcept { return value() + other; }

        constexpr T operator-(T other) const noexcept { return value() - other; }

        constexpr T operator/(T other) const noexcept { return value() / other; }

        constexpr T operator*(T other) const noexcept { return value() * other; }

        constexpr tagged_int& operator=(T other) noexcept
        {
            set(other);
            return *this;
        }

        constexpr tagged_int& operator+=(T other) noexcept
        {
            set(value() + other);
            return *this;
        }

        constexpr tagged_int& operator-=(T other) noexcept
        {
            set(value() - other);
            return *this;
        }

        constexpr tagged_int& operator*=(T other) noexcept
        {
            set(value() * other);
            return *this;
        }

        constexpr tagged_int& operator/=(T other) noexcept
        {
            set(value() / other);
            return *this;
        }
    };

    using tagged_u32 = tagged_int<u32, 1>;
    using tagged_u64 = tagged_int<u64, 1>;
}
