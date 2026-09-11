#pragma once

#include <types.hpp>

#include <assert2.hpp>

struct [[nodiscard]] result_error_t
{
    const char* message;
};

inline result_error_t error(const char* msg)
{
    return {msg};
}

template<class T>
struct [[nodiscard]] result
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
        : status(status::ok)
    {
        ::new (&value) T(v);
    }

    result(T&& v)
        : status(status::ok)
    {
        ::new (&value) T(static_cast<T&&>(v));
    }

    result(result_error_t e) noexcept
        : status(status::error)
        , message(e.message)
    {
    }

    ~result() noexcept { destroy_value(); }

    result(const result& rhs) noexcept
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

    result(result&& rhs) noexcept
        : status(rhs.status)
    {
        if (status == status::ok)
        {
            ::new (&value) T(static_cast<T&&>(rhs.value));
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

    result& operator=(result&& rhs) noexcept
    {
        if (this == &rhs)
        {
            return *this;
        }
        if (status == status::ok && rhs.status == status::ok)
        {
            value = static_cast<T&&>(rhs.value);
            return *this;
        }

        destroy_value();
        status = rhs.status;

        if (status == status::ok)
        {
            ::new (&value) T(static_cast<T&&>(rhs.value));
        }
        else
        {
            message = rhs.message;
        }
        return *this;
    }

    explicit operator bool() const noexcept { return status == status::ok; }

    T& operator*() &
    {
        assert2m(status == status::ok, message);
        return value;
    }

    T&& operator*() &&
    {
        assert2m(status == status::ok, message);
        return static_cast<T&&>(value);
    }

    const T& operator*() const&
    {
        assert2m(status == status::ok, message);
        return value;
    }

    T* operator->()
    {
        assert2m(status == status::ok, message);
        return &value;
    }

    const T* operator->() const
    {
        assert2m(status == status::ok, message);
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
