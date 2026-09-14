#pragma once

#include <job/fiber.hpp>
#include <job/scheduler.hpp>

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>

namespace job
{
    class condition_variable
    {
    public:
        explicit condition_variable(scheduler& sched);

        void notify_one();

        void notify_all();

        template<typename Lock, typename Pred>
        void wait(Lock& cv_lock, Pred&& pred)
        {
            if (pred())
            {
                return;
            }

            ++m_total_waiting;
            if (cpp::ref<fiber> current = m_scheduler.current_fiber())
            {
                // Mark ourselves as waiting on this CV.
                {
                    std::unique_lock lock(m_mutex);
                    m_waiting.emplace_back(current);
                }

                // Cooperatively yield until the predicate is satisfied.
                while (!pred())
                {
                    cv_lock.unlock();
                    m_scheduler.yield(current);
                    cv_lock.lock();
                }
            }
            else
            {
                // Not in a fiber context: use a traditional blocking CV wait.
                ++m_total_waiting_on_cv;
                m_cv.wait(cv_lock, std::forward<Pred>(pred));
                --m_total_waiting_on_cv;
            }
            --m_total_waiting;
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_cv;
        std::list<cpp::ref<fiber>> m_waiting;

        scheduler& m_scheduler;

        // Counters are used to avoid unnecessary notifications and to route wakeups.
        std::atomic<int> m_total_waiting {0};
        std::atomic<int> m_total_waiting_on_cv {0};
    };
}
