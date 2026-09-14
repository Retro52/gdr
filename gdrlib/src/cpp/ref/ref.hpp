#pragma once

#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

namespace cpp
{
    template<typename Type>
    struct ref
    {
        using type = Type;

        ref(type* p = nullptr) noexcept
            : m_ptr(p)
        {
            retain(this->m_ptr);
        }

        ref(std::nullptr_t) noexcept
            : m_ptr(nullptr)
        {
        }

        template<typename To, typename = std::enable_if_t<std::is_convertible_v<To*, type*>>>
        ref(const ref<To>& p) noexcept
            : m_ptr(p.get())
        {
            retain(this->m_ptr);
        }

        ref(const ref& p) noexcept
            : m_ptr(p.get())
        {
            retain(this->m_ptr);
        }

        ~ref() noexcept { revoke(this->m_ptr); }

        ref& operator=(type* p) noexcept
        {
            set(p);
            return *this;
        }

        ref& operator=(std::nullptr_t) noexcept
        {
            set(nullptr);
            return *this;
        }

        template<typename To, typename = std::enable_if_t<std::is_convertible_v<To*, type*>>>
        ref& operator=(const ref<To>& p) noexcept
        {
            set(p.get());
            return *this;
        }

        ref& operator=(const ref& p) noexcept
        {
            set(p.get());
            return *this;
        }

        type* get() const noexcept { return m_ptr; }

        operator type*() const noexcept { return get(); }

        type& operator*() const noexcept { return *get(); }

        type* operator->() const noexcept { return get(); }

        template<typename To>
        [[nodiscard]] bool is() const noexcept
        {
            if constexpr (std::is_same_v<Type, To> || std::is_base_of_v<To, Type>)
            {
                return true;
            }

            return as<To>() != nullptr;
        }

        template<typename To>
        [[nodiscard]] ref<To> as() const noexcept
        {
            if constexpr (std::is_same_v<Type, To> || std::is_base_of_v<To, Type>)
            {
                return ref<To>(static_cast<To*>(this->get()));
            }

            return ref<To>(dynamic_cast<To*>(this->get()));
        }

        void set(type* p) noexcept
        {
            if (this->m_ptr != p)
            {
                retain(p);
                revoke(this->m_ptr);

                this->m_ptr = p;
            }
        }

        type** release() noexcept
        {
            set(nullptr);
            return &this->m_ptr;
        }

        void swap(ref& p) noexcept { std::swap(this->m_ptr, p.m_ptr); }

        [[nodiscard]] bool is_unique() const noexcept { return use_count() == 1; }

        [[nodiscard]] std::size_t use_count() const noexcept
        {
            return this->m_ptr ? this->m_ptr->reference_count() : 0;
        }

        [[nodiscard]] bool operator==(type* p) const noexcept { return this->m_ptr == p; }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return this->m_ptr == nullptr; }

        [[nodiscard]] bool operator!=(type* p) const noexcept { return this->m_ptr != p; }

        [[nodiscard]] bool operator!=(std::nullptr_t) const noexcept { return this->m_ptr != nullptr; }

    private:
        static void retain(type* p) noexcept
        {
            if (p)
            {
                p->reference_add();
            }
        }

        static void revoke(type* p) noexcept
        {
            if (p)
            {
                p->reference_remove();
            }
        }

    private:
        type* m_ptr {nullptr};
    };

    template<typename T>
    inline auto get(T* x) noexcept
    {
        return x;
    }

    template<typename T>
    inline auto get(const ref<T>& x) noexcept
    {
        return x.get();
    }

    template<typename T>
    inline void swap(ref<T>& x, ref<T>& y) noexcept
    {
        x.swap(y);
    }

    template<typename T, typename... Args>
    [[nodiscard]] cpp::ref<T> make_ref(Args&&... args)
    {
        return new T(std::forward<Args>(args)...);
    }

    template<typename T>
    [[nodiscard]] inline cpp::ref<T> make_count_from_this(T* self) noexcept
    {
        return cpp::ref<T>(self);
    }
}

#if ENABLE_STL
template<typename T>
struct std::hash<::cpp::ref<T>>
{
    constexpr std::size_t operator()(const cpp::ref<T>& t) const noexcept
    {
        return std::hash<const void*> {}(static_cast<const void*>(t.get()));
    }
};
#endif
