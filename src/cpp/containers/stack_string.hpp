#pragma once

#include <assert2.hpp>
#include <cpp/alg_constexpr.hpp>
#include <cpp/hash/crc_hash.hpp>

#include <cstdio>

#if ENABLE_STL
#include <string>
#include <string_view>
#endif

namespace cpp
{
    template<u64 N>
    class stack_string_base;

    using stack_string     = stack_string_base<64>;
    using big_stack_string = stack_string_base<260>;

    template<u64 N>
    class stack_string_base
    {
    public:
        using char_type = char;

    public:
        constexpr stack_string_base() = default;

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const char* str)
            : stack_string_base(str, cpp::cx_strlen(str))
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
        [[maybe_unused]] constexpr stack_string_base(std::string_view sv)
            : stack_string_base(sv.data(), sv.length())
        {
        }

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const std::string& str)
            : stack_string_base(str.c_str(), str.length())
        {
        }
#endif

        explicit constexpr stack_string_base(const char* str, u64 len) { set_value(str, len); }

        constexpr void clear() { cpp::cx_fill(&m_str[0], m_str + N, 0); }

        [[nodiscard]] constexpr bool empty() const { return m_str[0] == 0; }

        [[nodiscard]] constexpr u64 length() const { return cpp::cx_strlen(m_str); }

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
            count = cpp::min(count, cpp::min(len - off, capacity() - 1));
            ret.set_value(m_str + off, count);

            return ret;
        }

        template<u64 No, typename... Args>
        static void format_to(stack_string_base<No>& dst, const char* fmt, Args&&... args)
        {
            stack_string_base<No> tmp;
            snprintf(tmp.data(), No, fmt, cpp::forward<Args>(args)...);
            dst.set_value(tmp.data(), tmp.length());
        }

        template<typename... Args>
        static auto make_formatted(const char* fmt, Args&&... args) noexcept
        {
            stack_string_base dst;
            format_to(dst, fmt, cpp::forward<Args>(args)...);

            return dst;
        }

        constexpr bool operator==(const char* other) const { return cpp::cx_strcmp(m_str, other) == 0; }

#if ENABLE_STL
        constexpr bool operator==(const std::string& other) const { return cpp::cx_strcmp(m_str, other.c_str()) == 0; }
#endif

        constexpr bool operator==(const stack_string_base& rhs) const { return cpp::cx_strcmp(m_str, rhs.m_str) == 0; }

        constexpr auto& operator=(const char* other)
        {
            set_value(other, cpp::cx_strlen(other));
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

        constexpr auto& operator+=(const char* other)
        {
            append_value(other, cpp::cx_strlen(other), this->length());
            return *this;
        }

        template<u64 No>
        constexpr auto& operator+=(const stack_string_base<No>& other)
        {
            append_value(other.c_str(), other.length(), this->length());
            return *this;
        }

        constexpr stack_string operator+(const char* other)
        {
            stack_string result;
            result += *this;
            result += other;
            return result;
        }

        template<u64 No>
        constexpr stack_string operator+(const stack_string_base<No>& other)
        {
            stack_string result;
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
            const auto size_clamped = cpp::min(N - 1, size);

            m_str[size_clamped] = 0;
            cpp::cx_copy_n(m_str, data, sizeof(char) * size_clamped);
        }

        constexpr void append_value(const char* data, const u64 size, u64 offset)
        {
            const auto size_clamped = cpp::min(N - 1 - offset, size);

            m_str[offset + size_clamped] = 0;
            cpp::cx_copy_n(m_str + offset, data, sizeof(char) * size_clamped);
        }

    private:
        char m_str[N] {};
    };
}

#if ENABLE_STL
template<u64 N>
struct std::hash<::cpp::stack_string_base<N>>
{
    constexpr auto operator()(const cpp::stack_string_base<N>& t) const noexcept
    {
        return ::cpp::crc::crc64(t.data(), t.length());
    }
};
#endif
