#pragma once

#include <types.hpp>

#include <cassert>
#include <type_traits>
#include <utility>

template<class T>
struct result
{
    enum class status
    {
        ok,
        error
    };

    status status;

    union
    {
        T value;
        const char* message {""};
    };

    result() noexcept
        : status(status::error)
    {
    }

    result(const T& v)
        requires(!std::is_same_v<std::remove_cvref_t<T>, const char*>)
        : status(status::ok)
    {
        ::new (&value) T(v);
    }

    result(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires(!std::is_same_v<std::remove_cvref_t<T>, const char*>)
        : status(status::ok)
    {
        ::new (&value) T(std::move(v));
    }

    result(const char* msg) noexcept
        : status(status::error)
        , message(msg)
    {
    }

    ~result() noexcept(std::is_nothrow_destructible_v<T>)
    {
        destroy_value();
    }

    result(const result& rhs)
        : status(rhs.status)
    {
        if (status == status::ok)
        {
            ::new (&value) T(rhs.value);
        }
        else
        {
            message = rhs.message;
        }
    }

    result(result&& rhs) noexcept(std::is_nothrow_move_constructible_v<T>)
        : status(rhs.status)
    {
        if (status == status::ok)
        {
            ::new (&value) T(std::move(rhs.value));
        }
        else
        {
            message = rhs.message;
        }
    }

    result& operator=(const result& rhs)
    {
        if (this == &rhs)
        {
            return *this;
        }
        if (status == status::ok && rhs.status == status::ok)
        {
            value = rhs.value;
            return *this;
        }

        destroy_value();
        status = rhs.status;

        if (status == status::ok)
        {
            ::new (&value) T(rhs.value);
        }
        else
        {
            message = rhs.message;
        }

        return *this;
    }

    result& operator=(result&& rhs) noexcept(std::is_nothrow_move_assignable_v<T>
                                             && std::is_nothrow_move_constructible_v<T>)
    {
        if (this == &rhs)
        {
            return *this;
        }
        if (status == status::ok && rhs.status == status::ok)
        {
            value = std::move(rhs.value);
            return *this;
        }

        destroy_value();
        status = rhs.status;

        if (status == status::ok)
        {
            ::new (&value) T(std::move(rhs.value));
        }
        else
        {
            message = rhs.message;
        }
        return *this;
    }

    explicit operator bool() const noexcept
    {
        return status == status::ok;
    }

    T& operator*() &
    {
        assert(status == status::ok);
        return value;
    }

    T operator*() &&
    {
        assert(status == status::ok);
        return value;
    }

    const T& operator*() const&
    {
        assert(status == status::ok);
        return value;
    }

    T* operator->()
    {
        assert(status == status::ok);
        return &value;
    }

    const T* operator->() const
    {
        assert(status == status::ok);
        return &value;
    }

private:
    void destroy_value()
    {
        if (status == status::ok)
        {
            value.~T();
        }
    }
};
