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

        template<typename Func, typename T>
        T extract(const char* name, T default_value, Func&& f) const noexcept
        {
            for (int i = 1; i < m_argc - 1; ++i)
            {
                if (cpp::cx_streq(name, m_argv[i]))
                {
                    return f(m_argv[i + 1]);
                }
            }

            return default_value;
        }

        template<typename T = cpp::stack_string>
        T read_string(const char* name, const char* default_value = "") const noexcept
        {
            return T {extract(name,
                              default_value,
                              [](auto&& value)
                              {
                                  return value;
                              })};
        }

        int read_numeric(const char* name, int default_value = 0) const noexcept
        {
            return extract(name, default_value, std::atoi);
        }

        float read_float(const char* name, float default_value = 0.0F) const noexcept
        {
            return extract(name, default_value, std::atof);
        }

        template<size_t N, typename T>
        glm::vec<N, T> read_vec(const char* name, glm::vec<N, T> default_value = glm::vec<N, T>(0))
        {
            static_assert(N > 1);

            return extract(name,
                           default_value,
                           [](auto&& value)
                           {
                               glm::vec<N, T> result;

                               u64 offset                  = 0;
                               const cpp::stack_string arg = value;
                               for (int j = 0; j < N - 1; ++j)
                               {
                                   const u64 next = arg.find_next(';', offset);
                                   result[j]      = std::atof(arg.substring(offset, next - offset - 1).c_str());

                                   offset = next + 1;
                               }

                               result[N - 1] = std::atof(arg.substring(offset, arg.length() - offset - 1).c_str());
                               return result;
                           });
        }

        vec2 read_vec2(const char* name, vec2 default_value = vec2(0.0F))
        {
            return read_vec<2, f32>(name, default_value);
        }

        vec3 read_vec3(const char* name, vec3 default_value = vec3(0.0F))
        {
            return read_vec<3, f32>(name, default_value);
        }

        vec4 read_vec4(const char* name, vec4 default_value = vec4(0.0F))
        {
            return read_vec<4, f32>(name, default_value);
        }
    };
}
