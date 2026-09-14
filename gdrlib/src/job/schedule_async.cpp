#include <job/schedule_async.hpp>
#include <job/wait_group.hpp>
#include <tracy/Tracy.hpp>

job::scheduler& job::get_scheduler()
{
    ZoneScoped;
    static job::scheduler scheduler;
    return scheduler;
}

void job::schedule_async(void (*task)(), const job::wait_group& wg)
{
    ZoneScoped;
    get_scheduler().schedule(
        [=]()
        {
            std::invoke(task);
        },
        wg
    );
}

void job::schedule_async(const std::function<void()>& task, const job::wait_group& wg)
{
    ZoneScoped;
    get_scheduler().schedule(
        [=]()
        {
            std::invoke(task);
        },
        wg
    );
}

void job::schedule_async_detached(void (*task)())
{
    ZoneScoped;
    get_scheduler().schedule(
        [=]()
        {
            std::invoke(task);
        }
    );
}

void job::schedule_async_detached(const std::function<void()>& task)
{
    ZoneScoped;
    get_scheduler().schedule(
        [=]()
        {
            std::invoke(task);
        }
    );
}
