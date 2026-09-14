#include <job/schedule_async.hpp>
#include <job/wait_group.hpp>
#include <tracy/Tracy.hpp>

using namespace job;

wait_group::wait_group()
    : wait_group(job::get_scheduler())
{
}

wait_group::wait_group(u64 initial_count)
    : wait_group(job::get_scheduler(), initial_count)
{
}

wait_group::wait_group(job::scheduler& scheduler)
    : m_data(cpp::make_ref<Data>(0, scheduler))
{
}

wait_group::wait_group(job::scheduler& scheduler, u64 initial_count)
    : m_data(cpp::make_ref<Data>(initial_count, scheduler))
{
}

void wait_group::done() const
{
    ZoneScoped;
    std::unique_lock lock(m_data->mutex);
    if (--m_data->count == 0)
    {
        m_data->cv.notify_all();
    }
}

void wait_group::add(u64 n)
{
    ZoneScoped;
    m_data->count += n;
}

u64 wait_group::count() const
{
    ZoneScoped;
    return m_data->count;
}

void wait_group::wait_till_done() const
{
    ZoneScoped;
    std::unique_lock lock(m_data->mutex);
    m_data->cv.wait(lock,
                    [this]()
                    {
                        return m_data->count.load() == 0;
                    });
}
