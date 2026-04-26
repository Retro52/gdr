#pragma once

#include <pod_types.hpp>

#include <functional>

struct janitor
{
private:
    std::function<void()> m_deferred;

public:
    template<typename Func>
    explicit janitor(Func&& func)
        requires(std::is_invocable_v<Func>)
        : m_deferred(std::forward<Func>(func))
    {
    }

    ~janitor() { m_deferred(); }
};

#define SUMMON_JANITOR(EXPR)                     \
    janitor EXPR_CONCAT(janitor_line_, __LINE__) \
    {                                            \
        [&]                                      \
        {                                        \
            ZoneScoped;                          \
            EXPR;                                \
        }                                        \
    }
