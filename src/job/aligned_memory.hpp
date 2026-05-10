#pragma once

#include <types.hpp>

namespace job
{
    class aligned_memory
    {
    public:
        aligned_memory() = default;

        // Non-copyable
        aligned_memory(const aligned_memory&)            = delete;
        aligned_memory& operator=(const aligned_memory&) = delete;

        aligned_memory(aligned_memory&& other) noexcept;
        aligned_memory& operator=(aligned_memory&& other) noexcept;

        aligned_memory(u64 size, u8 alignment);

        ~aligned_memory();

        [[nodiscard]] void* get() const noexcept;

        explicit operator void*() const noexcept;

    private:
        void* m_ptr {nullptr};
    };
}
