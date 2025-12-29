#pragma once

#include <types.hpp>

#include <cpp/containers/stack_string.hpp>
#include <fs/path_utils.hpp>

#include <filesystem>

namespace fs
{
    class path
    {
    public:
        constexpr path() = default;

        constexpr path(const char* p)
            : m_path(::fs::normalize_path(p, cpp::cx_strlen(p)))
        {
        }

        template<u64 N>
        constexpr path(const cpp::stack_string_base<N>& p)
            : m_path(::fs::normalize_path(p.c_str(), p.length()))
        {
        }

        path(const std::string& p)
            : m_path(::fs::normalize_path(p.c_str(), p.length()))
        {
        }

        path(const std::filesystem::path& fs_path)
            : path(fs_path.string())
        {
        }

        constexpr path& operator=(path other)
        {
            m_path = other.m_path;
            return *this;
        }

        [[nodiscard]] std::string string() const
        {
            return m_path.string();
        }

        [[nodiscard]] constexpr bool empty() const
        {
            return m_path.empty();
        }

        [[nodiscard]] constexpr u64 length() const
        {
            return m_path.length();
        }

        [[nodiscard]] constexpr const char* c_str() const
        {
            return m_path.c_str();
        }

        [[nodiscard]] constexpr path stem() const
        {
            return ::fs::stem(m_path);
        }

        [[nodiscard]] constexpr path back() const
        {
            return ::fs::back(m_path);
        }

        [[nodiscard]] constexpr path parent() const
        {
            return ::fs::parent(m_path);
        }

        [[nodiscard]] constexpr path basename() const
        {
            return ::fs::basename(m_path);
        }

        [[nodiscard]] constexpr path filename() const
        {
            return ::fs::filename(m_path);
        }

        [[nodiscard]] constexpr path extension() const
        {
            return ::fs::extension(m_path);
        }

        [[nodiscard]] static path current_path()
        {
            return std::filesystem::current_path();
        }

        constexpr path operator/(const path& other) const
        {
            return ::fs::join(m_path, other.m_path);
        }

        path operator/(const std::filesystem::path& other) const
        {
            return *this / path(other);
        }

        constexpr path operator/(const std::string& other) const
        {
            return *this / path(other.c_str());
        }

        constexpr path& operator+=(const path& other)
        {
            *this = *this / other;
            return *this;
        }

        bool operator==(const std::string& other) const
        {
            return m_path == other.c_str();
        }

        constexpr bool operator==(const path& other) const
        {
            return m_path == other.m_path;
        }

        constexpr bool operator!=(const path& other) const
        {
            return m_path != other.m_path;
        }

    private:
        path_string m_path;
    };
}

namespace std
{
    template<>
    struct hash<fs::path>
    {
        u64 operator()(const fs::path& path) const noexcept
        {
            return hash<string>()(path.string());
        }
    };
}
