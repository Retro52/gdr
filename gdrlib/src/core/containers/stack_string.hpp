#pragma once

#include <assert2.hpp>
#include <core/alg_constexpr.hpp>

#include <cstdio>

#if ENABLE_STL
#include <core/hash/crc_hash.hpp>

#include <string>
#include <string_view>
#endif

namespace core
{
    template<u64 N>
    class stack_string_base;

    using stack_string     = stack_string_base<64>;
    using big_stack_string = stack_string_base<260>;

    template<u64 N>
    class stack_string_base
    {
    public:
        using char_type           = char;
        constexpr static u64 npos = ~static_cast<u64>(0);

    public:
        constexpr stack_string_base() = default;

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const char* str)
            : stack_string_base(str, core::cx_strlen(str))
        {
        }

        template<u64 No>
        [[maybe_unused]] explicit constexpr stack_string_base(stack_string_base<No>&& other)
            : stack_string_base(other.c_str(), other.length())
        {
        }

        template<u64 No>
        [[maybe_unused]] explicit constexpr stack_string_base(const stack_string_base<No>& other)
            : stack_string_base(other.c_str(), other.length())
        {
        }

#if ENABLE_STL
        // String view support
        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(std::string_view sv)
            : stack_string_base(sv.data(), sv.length())
        {
        }

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const std::string& str)
            : stack_string_base(str.c_str(), str.length())
        {
        }
#endif

        explicit constexpr stack_string_base(const char* str, const u64 len) { set_value(str, len); }

        constexpr void clear() { core::cx_fill(&m_str[0], m_str + N, 0); }

        [[nodiscard]] constexpr bool empty() const { return m_str[0] == 0; }

        [[nodiscard]] constexpr u64 length() const { return core::cx_strlen(m_str); }

        [[nodiscard]] constexpr static u64 capacity() { return N; }

        [[nodiscard]] constexpr char* data() { return m_str; }

        [[nodiscard]] constexpr const char* data() const { return m_str; }

#if ENABLE_STL
        [[nodiscard]] std::string string() const { return std::string(m_str); }
#endif

        [[nodiscard]] constexpr const char* c_str() const { return data(); }

        [[nodiscard]] constexpr stack_string_base substring(const u64 off, u64 count) const
        {
            const u64 len = length();
            if (off >= len)
            {
                return {};
            }

            stack_string_base ret;
            count = core::min(count, core::min(len - off, capacity() - 1));
            ret.set_value(m_str + off, count);

            return ret;
        }

        [[nodiscard]] constexpr u64 find_next(const char c, const u64 off = 0) const
        {
            const u64 len = length();
            if (off >= len)
            {
                return {};
            }

            for (u64 i = off; i < off + len; ++i)
            {
                if (m_str[i] != c)
                {
                    continue;
                }

                return i;
            }

            return npos;
        }

        template<u64 No, typename... Args>
        static void format_to(stack_string_base<No>& dst, const char* fmt, Args&&... args)
        {
            stack_string_base<No> tmp;
            snprintf(tmp.data(), No, fmt, core::forward<Args>(args)...);
            dst.set_value(tmp.data(), tmp.length());
        }

        template<typename... Args>
        static auto make_formatted(const char* fmt, Args&&... args) noexcept
        {
            stack_string_base dst;
            format_to(dst, fmt, core::forward<Args>(args)...);

            return dst;
        }

        constexpr bool operator==(const char* other) const { return core::cx_strcmp(m_str, other) == 0; }

#if ENABLE_STL
        constexpr bool operator==(const std::string& other) const { return core::cx_strcmp(m_str, other.c_str()) == 0; }
#endif

        constexpr bool operator==(const stack_string_base& rhs) const { return core::cx_strcmp(m_str, rhs.m_str) == 0; }

        constexpr auto& operator=(const char* other)
        {
            set_value(other, core::cx_strlen(other));
            return *this;
        }

#if ENABLE_STL
        constexpr auto& operator=(const std::string& other)
        {
            set_value(other.c_str(), other.length());
            return *this;
        }
#endif

        template<u64 No>
        constexpr auto& operator=(const stack_string_base<No>& other)
        {
            if (&other == this)
            {
                return *this;
            }

            set_value(other.c_str(), other.length());
            return *this;
        }

        template<u64 No>
        constexpr auto& operator=(stack_string_base<No>&& other)
        {
            if (&other == this)
            {
                return *this;
            }

            set_value(other.c_str(), other.length());
            return *this;
        }

        constexpr auto& operator+=(const char other)
        {
            append_value(&other, 1, this->length());
            return *this;
        }

        constexpr auto& operator+=(const char* other)
        {
            append_value(other, core::cx_strlen(other), this->length());
            return *this;
        }

        template<u64 No>
        constexpr auto& operator+=(const stack_string_base<No>& other)
        {
            append_value(other.c_str(), other.length(), this->length());
            return *this;
        }

        constexpr auto operator+(const char* other) const
        {
            stack_string_base result;
            result += *this;
            result += other;
            return result;
        }

        template<u64 No>
        constexpr auto operator+(const stack_string_base<No>& other) const
        {
            stack_string_base<core::max(N, No)> result;
            result += *this;
            result += other;
            return result;
        }

        [[nodiscard]] constexpr char& operator[](const u64 pos)
        {
            assert2(pos < capacity() && "pos can not be bigger than capacity");
            return m_str[pos];
        }

        [[nodiscard]] constexpr const char& operator[](const u64 pos) const
        {
            assert2(pos < capacity() && "pos can not be bigger than capacity");
            return m_str[pos];
        }

    private:
        constexpr void set_value(const char* data, const u64 size)
        {
            const auto size_clamped = core::min(N - 1, size);

            m_str[size_clamped] = 0;
            core::cx_copy_n(m_str, data, sizeof(char) * size_clamped);
        }

        constexpr void append_value(const char* data, const u64 size, u64 offset)
        {
            const auto size_clamped = core::min(N - 1 - offset, size);

            m_str[offset + size_clamped] = 0;
            core::cx_copy_n(m_str + offset, data, sizeof(char) * size_clamped);
        }

    private:
        char m_str[N] {};
    };
}

constexpr auto operator""_stack(const char* str, const std::size_t size)
{
    return core::stack_string(str, size);
}

#if ENABLE_STL
template<u64 N>
struct std::hash<::core::stack_string_base<N>>
{
    constexpr auto operator()(const core::stack_string_base<N>& t) const noexcept
    {
        return ::core::hash::crc64(t.data(), t.length());
    }
};
#endif
