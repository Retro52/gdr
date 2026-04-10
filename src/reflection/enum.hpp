#pragma once

#include <assert2.hpp>
#include <cpp/containers/stack_string.hpp>
#include <pod_types.hpp>

#define REGISTER_ENUM(Name, ...)                                                        \
    enum class Name : u32                                                               \
    {                                                                                   \
        __VA_ARGS__                                                                     \
    };                                                                                  \
                                                                                        \
    constexpr u64 get_enum_values_count(const Name* /*enum*/)                           \
    {                                                                                   \
        return reflection::enum_values_count(#__VA_ARGS__);                             \
    }                                                                                   \
                                                                                        \
    inline const reflection::enum_value* get_enum_values(const Name* /* enum */)        \
    {                                                                                   \
        static const reflection::enum_value* values = []()                              \
        {                                                                               \
            constexpr static u64 count = reflection::get_enum_values_count<Name>();     \
            static cpp::stack_string names[count];                                      \
            static reflection::enum_value result[count];                                \
            static reflection::enum_counter<Name> __VA_ARGS__;                          \
            static u32 integer_values[] = {__VA_ARGS__};                                \
            reflection::parse_enum_values(#__VA_ARGS__, integer_values, names, result); \
            return result;                                                              \
        }();                                                                            \
                                                                                        \
        return values;                                                                  \
    }

namespace reflection
{
    struct enum_value
    {
        const char* name;
        u32 value;
    };

    template<typename EnumType>
    struct enum_counter
    {
        // NOLINTNEXTLINE(*-explicit-*)
        enum_counter(u32 v) noexcept
            : m_value(v)
        {
            s_counter = m_value + 1;
        }

        enum_counter() noexcept
            : m_value(s_counter)
        {
            s_counter = m_value + 1;
        }

        // NOLINTNEXTLINE(*-explicit-*)
        operator u32() const noexcept { return m_value; }

    private:
        u32 m_value;

        inline static u32 s_counter;
    };

    template<typename T>
    constexpr u64 get_enum_values_count()
    {
        return get_enum_values_count(static_cast<const T*>(nullptr));
    }

    template<typename T>
    const enum_value* get_enum_values()
    {
        return get_enum_values(static_cast<const T*>(nullptr));
    }

    template<typename T>
    u32 enum_from_string(const char* str)
    {
        const auto count       = get_enum_values_count<T>();
        const auto enum_values = get_enum_values<T>();

        for (int i = 0; i < count; ++i)
        {
            if (cpp::cx_streq(enum_values[i].name, str))
            {
                return enum_values[i].value;
            }
        }

        assert2m(false, "failed to convert string into an enum");
        return 0;
    }

    template<typename T>
    const char* string_from_enum(const T value)
    {
        const auto count       = get_enum_values_count<T>();
        const auto enum_values = get_enum_values<T>();

        for (int i = 0; i < count; ++i)
        {
            if (enum_values[i].value == static_cast<u32>(value))
            {
                return enum_values[i].name;
            }
        }

        assert2m(false, "failed to convert enum into a string");
        return "[unknown]";
    }

    constexpr u64 enum_values_count(const char* values)
    {
        u32 count     = 1;
        const char* c = &values[0];
        while (const char cursor = *(c++))
        {
            count = cursor == ',' ? count + 1 : count;
        }

        return count;
    }

    constexpr void parse_enum_values(const char* values, u32 enum_counters[], cpp::stack_string enum_names[],
                                     enum_value enum_values[])
    {
        bool skip              = false;
        u32 index              = 0;
        const char* str_ptr    = &values[0];
        const char* name_start = str_ptr;
        const char* name_end   = name_start;
        while (const char cursor = *(str_ptr++))
        {
            switch (cursor)
            {
            case ' ' :
            case '\t' :
            case '\n' :
            {
                if (name_start == name_end)
                {
                    ++name_end;
                    ++name_start;
                }
                break;
            }
            case '=' :
                skip = true;
                break;
            case ',' :
            case ';' :
                enum_names[index]  = cpp::stack_string(name_start, name_end - name_start);
                enum_values[index] = {.name  = enum_names[index].c_str(),
                                      .value = static_cast<u32>(enum_counters[index])};
                name_end = name_start = str_ptr;

                skip = false;
                ++index;
                break;
            default :
                name_end = skip ? name_end : ++name_end;
                break;
            }
        }

        if (name_start < name_end)
        {
            enum_names[index]  = cpp::stack_string(name_start, name_end - name_start);
            enum_values[index] = {.name = enum_names[index].c_str(), .value = static_cast<u32>(enum_counters[index])};
        }
    }
}
