#pragma once

#include <types.hpp>

#include <cpp/containers/stack_string.hpp>
#include <cpp/alg_constexpr.hpp>
#include <cpp/hash/crc_hash.hpp>

#if !defined(CPP_HS_STORE_SOURCE)
#if !defined(NDEBUG)
#define CPP_HS_STORE_SOURCE 1
#else
#define CPP_HS_STORE_SOURCE 0
#endif  // !defined(PRODUCTION_BUILD)
#endif  // !defined(CPP_HS_STORE_SOURCE)

namespace cpp
{
    struct hashed_string
    {
        hashed_string()
            : m_hash(0)
        {
        }

        hashed_string(const std::string& string)
            : hashed_string(string.c_str(), string.length())
        {
        }

        constexpr hashed_string(const char* str)
            : m_hash(crc::crc64(str, cpp::cx_strlen(str)))
#if CPP_HS_STORE_SOURCE
            , m_str(str)
#endif
        {
        }

        explicit constexpr hashed_string(const char* str, std::size_t len)
            : m_hash(crc::crc64(str, len))
#if CPP_HS_STORE_SOURCE
            , m_str(str, len)
#endif
        {
        }

        hashed_string(const hashed_string&)                = default;
        hashed_string(hashed_string&&) noexcept            = default;
        hashed_string& operator=(const hashed_string&)     = default;
        hashed_string& operator=(hashed_string&&) noexcept = default;

        ~hashed_string() = default;

        [[nodiscard]] constexpr bool empty() const
        {
            return m_hash == 0;
        }

        constexpr void clear()
        {
            m_hash = 0;
#if CPP_HS_STORE_SOURCE
            m_str.clear();
#endif
        }

        [[nodiscard]] constexpr u64 hash() const
        {
            return m_hash;
        }

        // NOLINTNEXTLINE(*-explicit-constructor)
        /* implicit */ constexpr operator u64() const
        {
            return m_hash;
        }

        bool operator==(const hashed_string& other) const
        {
            return m_hash == other.m_hash;
        }

        bool operator!=(const hashed_string& other) const
        {
            return m_hash != other.m_hash;
        }

        bool operator<(const hashed_string& other) const
        {
            return m_hash < other.m_hash;
        }

        constexpr static bool has_debug_string()
        {
#if CPP_HS_STORE_SOURCE
            return true;
#else
            return false;
#endif
        }

#if CPP_HS_STORE_SOURCE
        [[nodiscard]] std::string debug_string() const
        {
            return m_str.string();
        }
#else
        [[nodiscard]] std::string debug_string() const
        {
            return std::to_string(m_hash);
        }
#endif

    private:
        u64 m_hash;
#if CPP_HS_STORE_SOURCE
        cpp::stack_string m_str;
#endif
    };
}

// Operator to simplify string hashing. Example: using "str"_hs creates cpp::hashed_string from "str".
constexpr auto operator""_hs(const char* str, std::size_t size)
{
    return cpp::hashed_string(str, size);
}

template<>
struct std::hash<::cpp::hashed_string>
{
    constexpr std::size_t operator()(const cpp::hashed_string& t) const noexcept
    {
        return t.hash();
    }
};
