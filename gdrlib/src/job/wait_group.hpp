#pragma once

#include <core/ref/ref.hpp>
#include <core/ref/ref_counted.hpp>
#include <job/condition_variable.hpp>

#include <atomic>
#include <mutex>

namespace job
{
    class wait_group
    {
    public:
        explicit wait_group();

        explicit wait_group(u64 initial_count);

        explicit wait_group(job::scheduler& scheduler);

        explicit wait_group(job::scheduler& scheduler, u64 initial_count);

        void done() const;

        void add(u64 n);

        u64 count() const;

        void wait_till_done() const;

    private:
        struct Data : public core::ref_counted
        {
            explicit Data(u64 count, scheduler& scheduler)
                : cv(scheduler)
                , count(count)
            {
            }

            job::condition_variable cv;
            std::mutex mutex;
            std::atomic<u64> count;
        };

        core::ref<Data> m_data;
    };
}
