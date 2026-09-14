#pragma once

#include <pod_types.hpp>

#include <atomic>

namespace cpp
{
    struct ref_control_block
    {
        std::atomic<u64> weak_ref_counter   = 0;
        std::atomic<u64> strong_ref_counter = 0;
    };

    struct ref_counted
    {
        ref_counted() noexcept
            : m_ref_control_block(new ref_control_block)
        {
            ++(m_ref_control_block->weak_ref_counter);
        }

        ref_counted(const ref_counted& /* other */) noexcept
            : m_ref_control_block(new ref_control_block)
        {
            ++(m_ref_control_block->weak_ref_counter);
        }

        ref_counted(ref_counted&& other) noexcept
            : m_ref_control_block(other.m_ref_control_block)
        {
            other.m_ref_control_block = nullptr;
        }

        ref_counted&& operator=(ref_counted&&)     = delete;
        ref_counted& operator=(const ref_counted&) = delete;

        virtual ~ref_counted() noexcept
        {
            if (m_ref_control_block)
            {
                --(m_ref_control_block->weak_ref_counter);

                if (m_ref_control_block->weak_ref_counter == 0)
                {
                    delete m_ref_control_block;
                }

                m_ref_control_block = nullptr;
            }
        }

        u64 reference_add() const noexcept { return ++(m_ref_control_block->strong_ref_counter); }

        u64 reference_remove() const noexcept
        {
            const u64 cnt = --(m_ref_control_block->strong_ref_counter);

            if (cnt == 0)
            {
                delete this;
            }
            return cnt;
        }

        [[nodiscard]] u64 reference_count() const noexcept { return m_ref_control_block->strong_ref_counter; }

        [[nodiscard]] ref_control_block* get_control_block() const noexcept { return m_ref_control_block; }

        [[nodiscard]] u64 weak_reference_count() const noexcept { return m_ref_control_block->weak_ref_counter - 1; }

    private:
        ref_control_block* m_ref_control_block {nullptr};
    };
}
