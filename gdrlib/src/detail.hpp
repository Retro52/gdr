#pragma once

#include <type_traits>
#include <utility>

namespace detail
{
    template<class Tuple, class F, std::size_t... Is>
    constexpr void for_each_type_impl(F&& f, std::index_sequence<Is...>)
    {
        (f(std::type_identity<std::tuple_element_t<Is, Tuple>> {}), ...);
    }

    template<class Tuple, class F>
    constexpr void for_each_type(F&& f)
    {
        constexpr std::size_t N = std::tuple_size_v<Tuple>;
        for_each_type_impl<Tuple>(std::forward<F>(f), std::make_index_sequence<N> {});
    }
}
