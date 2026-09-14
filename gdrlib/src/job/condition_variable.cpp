#include <job/condition_variable.hpp>
#include <job/scheduler.hpp>
#include <tracy/Tracy.hpp>

using namespace job;

condition_variable::condition_variable(job::scheduler& sched)
    : m_scheduler(sched)
{
}

void condition_variable::notify_one()
{
    ZoneScoped;
    if (m_total_waiting == 0)
    {
        return;
    }

    // Wake up one waiting fiber
    {
        std::unique_lock lock(m_mutex);

        if (!m_waiting.empty())
        {
            const cpp::ref<fiber> f = m_waiting.front();
            m_waiting.pop_front();
            m_scheduler.enqueue(f);
        }
    }

    if (m_total_waiting_on_cv > 0)
    {
        m_cv.notify_one();
    }
}

void condition_variable::notify_all()
{
    ZoneScoped;
    if (m_total_waiting == 0)
    {
        return;
    }

    // Wake up all the sleeping fibers
    {
        std::unique_lock lock(m_mutex);
        while (!m_waiting.empty())
        {
            cpp::ref<fiber> f = m_waiting.front();
            m_waiting.pop_front();
            m_scheduler.enqueue(f);
        }
    }

    if (m_total_waiting_on_cv > 0)
    {
        m_cv.notify_all();
    }
}
