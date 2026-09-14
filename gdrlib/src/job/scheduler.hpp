#pragma once

#include <cpp/ref/ref.hpp>
#include <job/fiber.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace job
{
    class wait_group;

    class scheduler
    {
    private:
        using rng_func = std::function<u32()>;

    public:
        explicit scheduler();

        explicit scheduler(u64 thread_count);

        ~scheduler();

        void yield(const fiber* fiber);

        void enqueue(fiber* fiber);

        fiber* current_fiber();

        void schedule(const std::function<void()>& func);

        void schedule(const std::function<void()>& func, const wait_group& wg);

        void resize_thread_pool(u64 thread_count);

    private:
        struct task_queue
        {
            std::mutex mutex;
            std::condition_variable cv;
            std::queue<std::function<void()>> tasks;     ///< FIFO of plain tasks.
            std::queue<cpp::ref<fiber>> idle_fibers;     ///< Fibers parked after yielding.
            std::queue<cpp::ref<fiber>> waiting_fibers;  ///< Fibers ready to resume.
        };

        struct thread_state
        {
            cpp::ref<fiber> current {nullptr};  ///< Currently running fiber.
            cpp::ref<fiber> manager {nullptr};  ///< Manager (parking) fiber.
        };

        void worker_loop(u32 thread_id);

        void swap_from_current(fiber* to);

        void swap_to_manager(fiber* from);

        cpp::ref<fiber> create_worker(u32 thread_id);

        bool spin_work(u32 thread_id, const rng_func& rng);

        bool pop_local_queue(std::function<void()>& task, u32 thread_id);

        bool try_steal_task(std::function<void()>& out, u32 origin_thread_id, const rng_func& rng);

        bool try_acquire_task(std::function<void()>& out, u32 origin_thread_id, const rng_func& rng);

    private:
        // Protects scheduler-level data (e.g., thread id → worker id map).
        std::mutex m_sched_mutex;

        // Map OS thread id to worker index. Avoids thread_local due to platform issues.
        std::unordered_map<std::thread::id, u32> m_sys_tid_to_worker_id;

        // Arrays indexed by worker id.
        std::vector<task_queue> m_queues;
        std::vector<std::thread> m_threads;
        std::vector<thread_state> m_thread_states;

        std::atomic<u64> m_next_queue {0};  ///< Used to pick a queue in round-robin style

        std::atomic<bool> m_stop;
    };
}
