#pragma once

#include <fs/types.hpp>
#include <types.hpp>

namespace fs
{
    inline constexpr bool is_valid_separator(const char c)
    {
        return c == fs::path_separator || c == fs::native_separator;
    }

    [[nodiscard]] inline constexpr path_string strip(const path_string& p)
    {
        path_string copy = p;
        u64 pos          = copy.length();
        while (pos > 1 && is_valid_separator(copy[pos - 1]))
        {
            copy[--pos] = fs::path_terminator;
        }

        return copy;
    }

    inline constexpr path_string back(path_string p)
    {
        p = ::fs::strip(p);
        if (p.empty())
        {
            return {};
        }

        const u64 len = p.length();
        u64 pos       = p.length();

        while (pos > 0)
        {
            --pos;
            if (p[pos] == fs::path_separator)
            {
                return p.substring(pos + 1, len - (pos + 1));
            }
        }

        return p;
    }

    inline constexpr path_string stem(const path_string& p)
    {
        const auto file_name = ::fs::back(p);

        const u64 length = file_name.length();
        u64 pos          = length;
        while (pos > 0)
        {
            if (file_name[--pos] == '.')
            {
                return file_name.substring(0, pos);
            }
        }

        return file_name;
    }

    inline constexpr path_string parent(path_string p)
    {
        p = ::fs::strip(p);
        if (p.empty())
        {
            return p;
        }

        const u64 length = p.length();

        u64 pos = length;
        while (pos > 1 && p[pos - 1] != fs::path_separator)
        {
            --pos;
        }

        // If we got the root just return it; strip the trailing '/' otherwise. It's also possible we got an empty
        // filename, hence the '/' check
        pos = pos > 1 ? pos - 1 : (p[pos - 1] == '/' ? pos : length);
        return p.substring(0, pos);
    }

    inline constexpr path_string filename(const path_string& p)
    {
        return ::fs::back(p);
    }

    inline constexpr path_string basename(const path_string& p)
    {
        const auto basename = ::fs::filename(p);

        const u64 length = basename.length();
        u64 pos          = 0;
        while (pos < length)
        {
            if (basename[pos] == '.')
            {
                // Test for hidden files, aka files starting with the .
                return pos == 0 ? basename : basename.substring(0, pos);
            }

            pos++;
        }

        return basename;
    }

    inline constexpr path_string extension(const path_string& p)
    {
        const auto file_name = ::fs::filename(p);

        const u64 length = file_name.length();
        u64 pos          = length;
        while (pos > 0)
        {
            if (file_name[--pos] == '.')
            {
                // Test for hidden files, aka files starting with the .
                return pos == 0 ? path_string() : file_name.substring(pos, length - pos);
            }
        }

        return {};  // return an empty string for dummy files
    }

    inline constexpr path_string join(path_string p1, const path_string& p2)
    {
        if (p1.empty())
        {
            return fs::strip(p2);
        }

        if (p2.empty())
        {
            return fs::strip(p1);
        }

        p1 = fs::strip(p1);

        const u64 length = p1.length();
        if (length == path_string::capacity())
        {
            // Edge case: path overflow
            return p1;
        }

        u64 pos = length;
        if (p2[0] != fs::path_separator && p1[length - 1] != fs::path_separator)
        {
            p1[pos++] = fs::path_separator;
        }

        u64 off = 0;
        while (pos < p1.capacity() && off < p2.length())
        {
            p1[pos++] = p2[off++];
        }

        return fs::strip(p1);
    }

    inline constexpr path_string normalize_path(const char* path, const u64 len)
    {
        u64 pos = 0;
        path_string buffer;
        bool skip_next_separator = false;

        for (u64 i = 0; i < cpp::min(len, path_string::capacity() - 1); i++)
        {
            switch (const auto c = path[i])
            {
            case '/' :
            case '\\' :
                if (skip_next_separator)
                {
                    break;
                }
                skip_next_separator = true;
                buffer[pos++]       = ::fs::path_separator;
                break;
            default :
                buffer[pos++]       = c;
                skip_next_separator = false;
                break;
            }
        }

        buffer[pos] = '\0';
        return ::fs::strip(buffer);
    }
}
