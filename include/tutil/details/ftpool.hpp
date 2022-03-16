#pragma once
#include "tutil/details/base_tpool.hpp"

TUTIL_NAMESPACE_BEGIN_MAIN

/// [stack-allocated] [non-resizable] [non-movable] thread pool
template <size_t Size, template <typename> typename TaskQueue = details::deque>
class ftpool : private base_tpool<details::array<Size>::type, TaskQueue>
{
    using base_t = base_tpool<details::array<Size>::type, TaskQueue>;

public:
    using task_t = typename base_t::task_t;
    using mutex_t = typename base_t::mutex_t;
    using status_t = typename base_t::status_t;
    using priority_t = typename base_t::priority_t;
    using err_handler_t = typename base_t::err_handler_t;

public:
    ftpool()
    {
        for (auto &thread : threads_)
        {
            thread = std::thread{&ftpool::tinit_, this};
        }
    }

    ~ftpool()
    {
        {
            std::lock_guard<mutex_t> lock(task_mutex_);
            status_ = status_t::closed;
        }

        task_condition_.notify_all();
        for (auto &thread : threads_)
        {
            thread.join();
        }
    }

    using base_t::add;
    using base_t::count_idle;
    using base_t::count_working;
    using base_t::err_handler;
    using base_t::is_closed;
    using base_t::is_idle;
    using base_t::is_paused;
    using base_t::is_running;
    using base_t::pause;
    using base_t::pending_tasks;
    using base_t::resume;
    using base_t::size;
    using base_t::status;
    using base_t::wait;
    using base_t::wait_for;
    using base_t::wait_until;

protected:
    using base_t::add_;
    using base_t::get_;
    using base_t::invoke_err_handler_;

private:
    void tinit_() override
    {
        while (true)
        {
            task_t task;

            {
                status_t status_tmp;

                auto waitfunc = [&]
                {
                status_tmp = status();

                if (status_tmp == status_t::closed)
                    return true;
                if (status_tmp == status_t::paused)
                    return false;
                if (tasks_.empty())
                    return false;
                
                return true; };

                std::unique_lock<mutex_t> lock(task_mutex_);
                task_condition_.wait(lock, waitfunc);

                if (status_tmp == status_t::closed || tasks_.empty())
                {
                    // close thread pool
                    return;
                }

                task = get_();
            }

            working_count_++;
            task();
            working_count_--;

            // to awake the threads waiting on wait(), wait_for(), wait_until()
            {
                std::lock_guard<mutex_t> lock(task_mutex_);
                if (waitfunc_())
                {
                    wait_condition_.notify_all();
                }
            }
        }
    }
};

TUTIL_NAMESPACE_END_MAIN