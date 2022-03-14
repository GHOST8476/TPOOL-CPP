#pragma once
#include "tutil/details/worker.hpp"
#include <vector>

TUTIL_NAMESPACE_BEGIN_MAIN

/// heap based thread pool - resizable
/// tasks are evaluated based on their priority
template <template <typename> typename TaskQueue = details::deque>
class dynamic : private base_tpool<details::vector, TaskQueue>
{
    using base_t = base_tpool<details::vector, TaskQueue>;
    using worker_t = worker<TaskQueue>;

public:
    using task_t = typename base_t::task_t;
    using mutex_t = typename base_t::mutex_t;
    using status_t = typename base_t::status_t;
    using priority_t = typename base_t::priority_t;
    using err_handler_t = typename base_t::err_handler_t;

public:
    dynamic(size_t size = 0)
    {
        resize(size);
    }

    ~dynamic()
    {
        resize(0);
    }

    void tinit_() override
    {
        while (true)
        {
            task_t task;

            {
                status_t status_tmp;
                bool destroy_thread = false;

                auto waitfunc = [&]
                {
                status_tmp = status();
                destroy_thread = (actual_thread_count_ - thread_count_) > 0;

                if(destroy_thread)
                    return true;
                if (status_tmp == status_t::closed)
                    return true;
                if (status_tmp == status_t::paused)
                    return false;
                if (tasks_.empty())
                    return false;
                
                return true; };

                std::unique_lock<mutex_t> lock(task_mutex_);
                task_condition_.wait(lock, waitfunc);

                // destroy this thread
                if (destroy_thread)
                {
                    // auto thread_remove_task = [&threads_, &invoke_err_handler_](std::thread::id id) -> bool
                    auto thread_remove_task = [&](std::thread::id id) -> bool
                    {
                        auto it = std::find_if(threads_.begin(), threads_.end(), [&id](const std::thread &thread)
                                               { return thread.get_id() == id; });

                        if (it == threads_.end())
                        {
                            invoke_err_handler_("failed destroying thread");
                            return false;
                        }

                        threads_.erase(it);
                        return true;
                    };

                    // join this thread in worker thread
                    worker_.add(thread_remove_task, std::this_thread::get_id());
                    actual_thread_count_--;
                    return;
                }

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

    void resize(size_t size) TUTIL_NOEXCEPT
    {
        std::lock_guard<mutex_t> lock(task_mutex_);
        if (size == thread_count_)
        {
            return;
        }

        if (size > thread_count_)
        {
            size_t count = size - thread_count_;
            worker_.add([this, count]
                        {
                            for (size_t i = 0; i < count; i++)
                                { this->threads_.emplace_back(&dynamic::tinit_, this); } });

            thread_count_ = size;
            return;
        }

        if (size < thread_count_)
        {
            for (size_t i = 0; i < thread_count_ - size; i++)
            {
                task_condition_.notify_one();
            }

            thread_count_ = size;
            return;
        }
    }

    size_t size() const TUTIL_NOEXCEPT
    {
        std::lock_guard<mutex_t> lock(task_mutex_);
        return thread_count_;
    }

    size_t acutal_size() const TUTIL_NOEXCEPT
    {
        return base_t::size();
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
    using base_t::status;
    using base_t::wait;
    using base_t::wait_for;
    using base_t::wait_until;

protected:
    size_t actual_thread_count_ = 0;
    size_t thread_count_ = 0;
    worker_t worker_;
};

/// movable version of dynamic
template <template <typename> typename TaskQueue = details::deque,
          template <typename> typename Allocator = details::allocator>
class dynamic_mv : private base_tpool_mv<dynamic<TaskQueue>, Allocator>
{
    using base_t = base_tpool_mv<dynamic<TaskQueue>, Allocator>;

public:
    using task_t = typename base_t::task_t;
    using mutex_t = typename base_t::mutex_t;
    using status_t = typename base_t::status_t;
    using priority_t = typename base_t::priority_t;
    using err_handler_t = typename base_t::err_handler_t;

public:
    dynamic_mv(size_t size = 0) : base_t(size) {}

    size_t resize(size_t size) TUTIL_NOEXCEPT
    {
        return base_t::validate().resize(size);
    }

    size_t actual_size() const TUTIL_NOEXCEPT
    {
        return base_t::validate().actual_size();
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
};

TUTIL_NAMESPACE_END_MAIN