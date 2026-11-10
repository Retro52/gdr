#include <tracy/Tracy.hpp>
#include <utils.hpp>

#include <cmath>

u64 utils::align_divide(const u64 number, const u64 alignment)
{
    return (number + alignment - 1) / alignment;
}

core::stack_string utils::format_bytes_number(u64 number)
{
    ZoneScoped;
    if (number < 1024)
    {
        return core::stack_string::make_formatted("%llu.000 B", number);
    }

    constexpr const char* magnitudes[] = {"KB", "MB", "GB", "TB"};

    const auto magnitude = static_cast<i32>(std::log2(number) / 10);
    const f64 fraction   = static_cast<f64>(number) / std::pow(1024, magnitude);
    return core::stack_string::make_formatted("%.3lf %s", fraction, magnitudes[magnitude - 1]);
}

core::stack_string utils::format_counter_number(u64 number)
{
    ZoneScoped;
    if (number < 1000)
    {
        return core::stack_string::make_formatted("%llu.000", number);
    }

    constexpr const char* magnitudes[] = {"K", "M", "B", "T"};

    const auto magnitude = static_cast<i32>(std::log10(number) / 3);
    const f64 fraction   = static_cast<f64>(number) / std::pow(1000, magnitude);
    return core::stack_string::make_formatted("%.3lf %s", fraction, magnitudes[magnitude - 1]);
}
