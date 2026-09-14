#pragma once

#include <cpp/containers/stack_string.hpp>
#include <cpp/ref/ref_counted.hpp>
#include <job/aligned_memory.hpp>
#include <job/fibers/fiber_api.h>

#include <functional>

#if defined(_WIN32)
#define JOB_NO_STACK_PTR
#endif

namespace job
{
    class fiber : public cpp::ref_counted
    {
    public:
        using run_func = std::function<void(fiber* self)>;

    public:
        fiber();

        ~fiber() override;

        explicit fiber(u32 stack_size, u32 thread_id);

        explicit fiber(u32 stack_size, u32 thread_id, run_func run);

        void swap(const fiber* to);

        void set_run(run_func func);

        [[nodiscard]] u32 thread_id() const;

#if TRACY_ENABLE
        [[nodiscard]] const char* get_tracy_name() const noexcept;

        void set_tracy_name(const cpp::stack_string& name) noexcept;
#endif

    private:
        [[nodiscard]] void* stack_ptr() const;

        static void run(void* target);

    private:
#if TRACY_ENABLE
        cpp::stack_string m_tracy_name;
#endif
        std::function<void(fiber* self)> m_run;
        fiber_registers_t* m_registers {};
#if !defined(JOB_NO_STACK_PTR)
        aligned_memory m_stack_ptr;
#endif
        u32 m_stack_size {0};
        u32 m_thread_id {0};
    };
}
