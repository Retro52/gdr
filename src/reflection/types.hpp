#pragma once

#include <cpp/containers/heap_array.hpp>

namespace reflection
{
    enum class types
    {
        i8,
        u8,
        i16,
        u16,
        i32,
        u32,
        i64,
        u64,
        f32,
        f64,
        structure,
        enumeration,
    };

    struct enum_declaration;
    struct struct_declaration;

    union decl_wrapper
    {
        u64 dummy {0};
        enum_declaration* enum_decl;
        struct_declaration* struct_decl;
    };

    struct value_declaration
    {
        types type;
        const char* name;
        decl_wrapper declaration;
    };

    struct enum_value
    {
        const char* name;
        u32 value;
    };

    struct enum_declaration
    {
        cpp::heap_array<enum_value> values;
        const char* name;
    };

    struct struct_declaration
    {
        cpp::heap_array<value_declaration> fields;
        const char* name;
    };
}
