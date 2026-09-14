#pragma once

#include <job/scheduler.hpp>

#include <functional>

namespace job
{
    class wait_group;

    scheduler& get_scheduler();

    void schedule_async(void (*task)(), const job::wait_group& wg);

    void schedule_async(const std::function<void()>& task, const job::wait_group& wg);

    void schedule_async_detached(void (*task)());

    void schedule_async_detached(const std::function<void()>& task);
}
