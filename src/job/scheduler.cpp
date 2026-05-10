#include <types.hpp>

#include <cpp/containers/stack_string.hpp>
#include <janitor.hpp>
#include <job/scheduler.hpp>
#include <job/wait_group.hpp>
#include <tracy/Tracy.hpp>

#include <cassert>
#include <chrono>
#include <random>

#if defined(_WIN32)
#include <intrin.h>  // __nop()
#endif

using namespace job;

namespace
{
    inline void nop()
    {
#if defined(_WIN32)
        __nop();
#else
        __asm__ __volatile__("nop");
#endif
    }
}

scheduler::scheduler()
{
    resize_thread_pool(std::thread::hardware_concurrency());
}

scheduler::scheduler(const u64 thread_count)
{
    resize_thread_pool(thread_count);
}

scheduler::~scheduler()
{
    resize_thread_pool(0);
}

void scheduler::yield(const fiber* fiber)
{
    ZoneScoped;
    const auto fiber_tid = fiber->thread_id();

    assert(m_thread_states[fiber_tid].current == fiber);
    if (fiber != m_thread_states[fiber_tid].manager)
    {
        auto& queue = m_queues[fiber_tid];
        cpp::ref<job::fiber> worker;

        {
            std::unique_lock lock(queue.mutex);

            if (!queue.waiting_fibers.empty())
            {
                worker = queue.waiting_fibers.front();
                queue.waiting_fibers.pop();
            }
            else if (!queue.idle_fibers.empty())
            {
                worker = queue.idle_fibers.front();
                queue.idle_fibers.pop();
            }
            else if (!queue.tasks.empty())
            {
                worker = create_worker(fiber_tid);
            }
        }

        if (worker)
        {
            swap_from_current(worker);
            return;
        }
    }
}

void scheduler::enqueue(fiber* fiber)
{
    ZoneScoped;
    const auto fiber_thread = fiber->thread_id();
    auto& queue             = m_queues[fiber_thread];

    std::unique_lock lock(queue.mutex);
    if (fiber != m_thread_states[fiber_thread].manager)
    {
        queue.waiting_fibers.emplace(fiber);
        queue.cv.notify_one();
    }
    else
    {
        assert(
            false
            && "How did you do that? This could only mean that somehow manager is locked at the CV, which is both stupid and impressive");
    }
}

fiber* scheduler::current_fiber()
{
    ZoneScoped;
    std::unique_lock lock(m_sched_mutex);

    const auto worker_id_iter = m_sys_tid_to_worker_id.find(std::this_thread::get_id());
    if (worker_id_iter != m_sys_tid_to_worker_id.end())
    {
        return m_thread_states[worker_id_iter->second].current;
    }

    return nullptr;
}

void scheduler::schedule(const std::function<void()>& func)
{
    ZoneScoped;
    const u64 idx = m_next_queue.fetch_add(1, std::memory_order_relaxed) % m_queues.size();
    std::unique_lock lock(m_queues[idx].mutex);
    m_queues[idx].tasks.emplace(func);
    m_queues[idx].cv.notify_one();
}

void scheduler::schedule(const std::function<void()>& func, const job::wait_group& wg)
{
    ZoneScoped;
    const auto run = [func, wg]()
    {
        SUMMON_JANITOR(wg.done());
        func();
    };

    const u64 idx = m_next_queue.fetch_add(1, std::memory_order_relaxed) % m_queues.size();
    std::unique_lock lock(m_queues[idx].mutex);
    m_queues[idx].tasks.emplace(run);
    m_queues[idx].cv.notify_one();
}

void scheduler::resize_thread_pool(u64 thread_count)
{
    ZoneScoped;
    if (thread_count == m_threads.size())
    {
        return;
    }

    if (thread_count == 0)
    {
        m_stop = true;
        for (auto& queue : m_queues)
        {
            queue.cv.notify_one();
        }

        // Join all threads
        for (auto& th : m_threads)
        {
            if (th.joinable())
            {
                th.join();
            }
        }

        m_queues.clear();
        m_threads.clear();
        m_thread_states.clear();
    }
    else
    {
        // Note (Anton): resize_thread_pool only supports thread re-creation and total shutdown.
        // This is done to make sure we never loose any work that might be in progress by other threads.
        // If we were to support shrinking the thread pool, we'd have to somehow re-assign all the work that is being
        // done by them, and that's just too edgy to do.
        if (!m_threads.empty())
        {
            assert(
                false
                && "resize_tread_pool only supports thread pool re-creation. see the note above in the source code.");
            return;
        }

        m_stop = false;

        m_thread_states.resize(thread_count);
        m_queues = std::vector<queue_t>(thread_count);

        for (u32 i = 0; i < thread_count; i++)
        {
            m_threads.emplace_back(&scheduler::worker_loop, this, i);
        }
    }
}

void scheduler::worker_loop(const u32 thread_id)
{
    const auto str = cpp::stack_string::make_formatted("Worker %u", thread_id);
    tracy::SetThreadName(str.c_str());

    ZoneScoped;
    // NOTE: we can abuse a free-lock behaviour here because we know that worker loop is called upon scheduler creation,
    // hence no other thread would try to use ours data
    {
        std::lock_guard lock(m_sched_mutex);
        m_sys_tid_to_worker_id[std::this_thread::get_id()] = thread_id;
    }

    // manager fiber is just a stub for default worker_loop registers state (aka context)
    const cpp::ref<fiber> worker  = create_worker(thread_id);
    const cpp::ref<fiber> manager = cpp::make_ref<fiber>();

    m_thread_states[thread_id] = {worker, manager};
    manager->swap(worker);
}

void scheduler::swap_from_current(fiber* to)
{
    ZoneScoped;
    auto& [current, _] = m_thread_states[to->thread_id()];

    const auto current_copy = current;
    current                 = to;
    current_copy->swap(to);
}

void scheduler::swap_to_manager(fiber* from)
{
    ZoneScoped;
    const auto tid           = from->thread_id();
    auto& queue              = m_queues[tid];
    auto& [current, manager] = m_thread_states[tid];

    assert(from == current);

    {
        std::unique_lock lock(queue.mutex);
        queue.idle_fibers.emplace(from);
    }

    const auto current_copy = current;
    current                 = manager;
    current_copy->swap(manager);
}

cpp::ref<fiber> scheduler::create_worker(u32 thread_id)
{
    ZoneScoped;
    // 1MB for a stack is more than enough
    const cpp::ref<fiber> worker = cpp::make_ref<fiber>(
        1_MB,
        thread_id,
        [this, thread_id](auto* self)
        {
            SUMMON_JANITOR(swap_to_manager(self));
            std::mt19937 rng(static_cast<unsigned>(std::hash<std::thread::id>()(std::this_thread::get_id())));
            std::uniform_int_distribution<u32> dist(0, static_cast<u32>(m_threads.size() - 1));

            const auto rng_generator = [&rng, &dist]
            {
                return dist(rng);
            };

            while (spin_work(thread_id, rng_generator))
            {
            }
        });

#if TRACY_ENABLE
    worker->set_tracy_name(cpp::stack_string::make_formatted("Fiber for %u", thread_id));
#endif

    return worker;
}

bool scheduler::spin_work(const u32 thread_id, const scheduler::rng_func& rng)
{
    ZoneScoped;
    if (m_stop)
    {
        return false;
    }

    std::function<void()> task  = nullptr;
    cpp::ref<fiber> queue_fiber = nullptr;

    auto& queue = m_queues[thread_id];

    // NOTE (Anton): Original idea belongs to the MARL Authors.
    // Code snippet from file 'src/scheduler.cpp' is used under APACHE-2.0 License.
    // Changes to original code snippet include minor stylistic and structural changes.
    constexpr auto duration = std::chrono::milliseconds(1);
    auto start              = std::chrono::high_resolution_clock::now();
    while (std::chrono::high_resolution_clock::now() - start < duration)
    {
        for (int i = 0; i < 256; i++)
        {
            // clang-format off
            nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
            nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
            nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
            nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
            // clang-format on

            if (!queue.tasks.empty() || !queue.waiting_fibers.empty())
            {
                std::unique_lock lock(queue.mutex);
                if (!queue.waiting_fibers.empty())
                {
                    queue_fiber = queue.waiting_fibers.front();
                    queue.waiting_fibers.pop();
                    queue.idle_fibers.emplace(m_thread_states[thread_id].current);
                    break;
                }

                if (!queue.tasks.empty())
                {
                    task = std::move(queue.tasks.front());
                    queue.tasks.pop();
                    break;
                }
            }
        }

        assert(!(queue_fiber && task)
               && "We can not do a few things at once, so we should never get both queue_fiber and a task from here!");

        if (queue_fiber || task || try_steal_task(task, thread_id, rng))
        {
            break;
        }

        std::this_thread::yield();
    }

    if (!queue_fiber && !task)
    {
        std::unique_lock lock(m_queues[thread_id].mutex);
        m_queues[thread_id].cv.wait(lock,
                                    [&]()
                                    {
                                        return !m_queues[thread_id].tasks.empty()
                                            || !m_queues[thread_id].waiting_fibers.empty() || m_stop;
                                    });
    }

    if (!queue_fiber && !task)
    {
        std::unique_lock lock(queue.mutex);

        if (!queue.waiting_fibers.empty())
        {
            queue_fiber = queue.waiting_fibers.front();
            queue.waiting_fibers.pop();
            queue.idle_fibers.emplace(m_thread_states[thread_id].current);
        }
    }

    if (queue_fiber)
    {
        swap_from_current(queue_fiber);
        return true;
    }

    if (!task && !try_acquire_task(task, thread_id, rng))
    {
        return true;  // no task, spin around
    }

    task();
    return true;
}

bool scheduler::pop_local_queue(std::function<void()>& task, u32 thread_id)
{
    ZoneScoped;
    std::unique_lock lock(m_queues[thread_id].mutex);

    if (!m_queues[thread_id].tasks.empty())
    {
        task = std::move(m_queues[thread_id].tasks.front());
        m_queues[thread_id].tasks.pop();
        return true;
    }

    return false;
}

bool scheduler::try_steal_task(std::function<void()>& out, u32 origin_thread_id, const scheduler::rng_func& rng)
{
    ZoneScoped;
    constexpr u64 max_attempts = 3;
    for (u64 attempt = 0; attempt < max_attempts; attempt++)
    {
        const u32 to_steal_from_id = rng();
        if (origin_thread_id == to_steal_from_id)
        {
            continue;
        }

        if (pop_local_queue(out, to_steal_from_id))
        {
            return true;
        }
    }

    return false;
}

bool scheduler::try_acquire_task(std::function<void()>& out, u32 origin_thread_id, const scheduler::rng_func& rng)
{
    ZoneScoped;
    return pop_local_queue(out, origin_thread_id) || try_steal_task(out, origin_thread_id, rng);
}
