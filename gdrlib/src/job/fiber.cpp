#include <job/fiber.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>

using namespace job;

fiber::fiber()
{
    ZoneScoped;
    m_registers = fiber_init_manager();
}

fiber::~fiber()
{
    ZoneScoped;
    if (!m_stack_size)
    {
        fiber_destroy_manager(m_registers);
        return;
    }

    fiber_destroy(m_registers);
}

fiber::fiber(u32 stack_size, u32 thread_id)
    : fiber(stack_size, thread_id, nullptr)
{
    ZoneScoped;
}

fiber::fiber(u32 stack_size, u32 thread_id, run_func run)
    : m_run(std::move(run))
#if !defined(JOB_NO_STACK_PTR)
    , m_stack_ptr(stack_size, 16)
#endif
    , m_stack_size(stack_size)
    , m_thread_id(thread_id)
{
    ZoneScoped;
    assert(
        stack_size > 0
        && "empty stack is not allowed, as all of the operating systems require at least a few bytes of spare space");

    if (m_run)
    {
        m_registers = fiber_init(stack_ptr(), m_stack_size, &fiber::run, this);
    }
}

void fiber::swap(const fiber* to)
{
    ZoneScoped;

#if TRACY_ENABLE
    TracyFiberEnter(to->get_tracy_name());
#endif

    fiber_swap(m_registers, to->m_registers);
}

void fiber::set_run(run_func func)
{
    ZoneScoped;
    m_run = std::move(func);

    assert(m_run && "nullptr is not allowed in explicit set_run calls");
    m_registers = fiber_init(stack_ptr(), m_stack_size, &fiber::run, this);
}

u32 fiber::thread_id() const
{
    ZoneScoped;
    return m_thread_id;
}

#if TRACY_ENABLE
const char* fiber::get_tracy_name() const noexcept
{
    return m_tracy_name.c_str();
}

void fiber::set_tracy_name(const cpp::stack_string& name) noexcept
{
    m_tracy_name = name;
}
#endif

void* fiber::stack_ptr() const
{
#if defined(JOB_NO_STACK_PTR)
    return nullptr;
#else
    return m_stack_ptr.get();
#endif
}

void fiber::run(void* target)
{
    ZoneScoped;
    static_cast<fiber*>(target)->m_run(static_cast<fiber*>(target));
}
