#pragma once

#include <cpp/alg_constexpr.hpp>
#include <cpp/containers/stack_string.hpp>

#include <cstdlib>

namespace app
{
    struct argv_handler
    {
    private:
        int m_argc;
        char** m_argv;

    public:
        argv_handler(int argc, char** argv)
            : m_argc(argc)
            , m_argv(argv)
        {
        }

        int get_positional_args_start() const noexcept
        {
            for (int i = 1; i < m_argc; ++i)
            {
                if (cpp::cx_strlen(m_argv[i]) > 2 && m_argv[i][0] == '-')
                {
                    ++i;
                }
                else
                {
                    return i;
                }
            }

            return -1;
        }

        template<typename T = cpp::stack_string>
        T read_string(const char* name, const char* default_value = "") const noexcept
        {
            for (int i = 1; i < m_argc - 1; ++i)
            {
                if (cpp::cx_streq(name, m_argv[i]))
                {
                    return T {m_argv[i + 1]};
                }
            }

            return T {default_value};
        }

        int read_numeric(const char* name, int default_value = 0) const noexcept
        {
            for (int i = 1; i < m_argc - 1; ++i)
            {
                if (cpp::cx_streq(name, m_argv[i]))
                {
                    return atoi(m_argv[i + 1]);
                }
            }

            return default_value;
        }
    };
}
