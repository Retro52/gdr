#pragma once

#include <cpp/alg_constexpr.hpp>
#include <cpp/hash/crc_hash.hpp>
#include <cpp/math.hpp>

#include <cassert>
#include <string>
#include <string_view>

namespace cpp
{
    template<size_t N>
    class stack_string_base;

    using stack_string     = stack_string_base<64>;
    using big_stack_string = stack_string_base<260>;

    template<size_t N>
    class stack_string_base
    {
    public:
        using char_type = char;

    public:
        constexpr stack_string_base() = default;

        // String view support
        // NOLINTNEXTLINE(*-explicit-constructor)
        [[maybe_unused]] constexpr stack_string_base(std::string_view sv)
            : stack_string_base(sv.data(), sv.length())
        {
        }

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const char* str)
            : stack_string_base(str, cpp::cx_strlen(str))
        {
        }

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ [[maybe_unused]] constexpr stack_string_base(const std::string& str)
            : stack_string_base(str.c_str(), str.length())
        {
        }

        template<size_t No>
        [[maybe_unused]] explicit constexpr stack_string_base(stack_string_base<No>&& other)
            : stack_string_base(other.c_str(), other.length())
        {
        }

        template<size_t No>
        [[maybe_unused]] explicit constexpr stack_string_base(const stack_string_base<No>& other)
            : stack_string_base(other.c_str(), other.length())
        {
        }

        explicit constexpr stack_string_base(const char* str, size_t len)
        {
            set_value(str, len);
        }

        constexpr void clear()
        {
            cpp::cx_fill(&m_str[0], m_str + N, 0);
        }

        [[nodiscard]] constexpr bool empty() const
        {
            return m_str[0] == 0;
        }

        [[nodiscard]] constexpr size_t length() const
        {
            return cpp::cx_strlen(m_str);
        }

        [[nodiscard]] constexpr static size_t capacity()
        {
            return N;
        }

        [[nodiscard]] constexpr char* data()
        {
            return m_str;
        }

        [[nodiscard]] constexpr const char* data() const
        {
            return m_str;
        }

        [[nodiscard]] std::string string() const
        {
            return std::string(m_str);
        }

        [[nodiscard]] constexpr const char* c_str() const
        {
            return data();
        }

        [[nodiscard]] constexpr stack_string_base substring(const size_t off, size_t count) const
        {
            const size_t len = length();
            if (off >= len)
            {
                return {};
            }

            stack_string_base ret;
            count = cpp::min(count, cpp::min(len - off, ret.capacity() - 1));
            ret.set_value(m_str + off, count);

            return ret;
        }

        template<size_t No, typename... Args>
        static void format_to(stack_string_base<No>& dst, const char* fmt, Args&&... args)
        {
            snprintf(dst.data(), No, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static auto make_formatted(const char* fmt, Args&&... args) noexcept
        {
            stack_string_base dst;
            format_to(dst, fmt, std::forward<Args>(args)...);

            return dst;
        }

        constexpr bool operator==(const char* other) const
        {
            return cpp::cx_strcmp(m_str, other) == 0;
        }

        constexpr bool operator==(const std::string& other) const
        {
            return cpp::cx_strcmp(m_str, other.c_str()) == 0;
        }

        constexpr bool operator==(const stack_string_base& rhs) const
        {
            return cpp::cx_strcmp(m_str, rhs.m_str) == 0;
        }

        constexpr auto& operator=(const char* other)
        {
            set_value(other, cpp::cx_strlen(other));
            return *this;
        }

        constexpr auto& operator=(const std::string& other)
        {
            set_value(other.c_str(), other.length());
            return *this;
        }

        template<size_t No>
        constexpr auto& operator=(const stack_string_base<No>& other)
        {
            if (&other == this)
            {
                return *this;
            }

            set_value(other.c_str(), other.length());
            return *this;
        }

        template<size_t No>
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

        template<size_t No>
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

        template<size_t No>
        constexpr stack_string operator+(const stack_string_base<No>& other)
        {
            stack_string result;
            result += *this;
            result += other;
            return result;
        }

        [[nodiscard]] constexpr char& operator[](const size_t pos)
        {
            assert(pos < capacity() && "pos can not be bigger than capacity");
            return m_str[pos];
        }

        [[nodiscard]] constexpr const char& operator[](const size_t pos) const
        {
            assert(pos < capacity() && "pos can not be bigger than capacity");
            return m_str[pos];
        }

    private:
        constexpr void set_value(const char* data, size_t size)
        {
            const auto size_clamped = std::min(N - 1, size);

            m_str[size_clamped] = 0;
            cpp::cx_copy_n(m_str, data, sizeof(char) * size_clamped);
        }

        constexpr void append_value(const char* data, size_t size, size_t offset)
        {
            const auto size_clamped = std::min(N - 1 - offset, size);

            m_str[offset + size_clamped] = 0;
            cpp::cx_copy_n(m_str + offset, data, sizeof(char) * size_clamped);
        }

    private:
        char m_str[N] {};
    };
}

template<size_t N>
struct std::hash<::cpp::stack_string_base<N>>
{
    constexpr std::size_t operator()(const cpp::stack_string_base<N>& t) const noexcept
    {
        return ::cpp::crc::crc64(t.data(), t.length());
    }
};
