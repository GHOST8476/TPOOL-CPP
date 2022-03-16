#pragma once

#include "tpool/details/common.hpp"
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <exception>
#include <atomic>
#include <typeinfo>

TUTIL_NAMESPACE_BEGIN_MAIN

/// base thread pool
/// tasks are evaluated based on their priority
template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
class base_tpool
{
    using this_t = base_tpool<ThreadContainer, TaskContainer>;

    template <typename T>
    using lambda_mover_t = details::lambda_mover<T>;
    template <typename T>
    using thread_container_t = ThreadContainer<T>;
    template <typename T>
    using task_container_t = TaskContainer<T>;

public:
    using task_t = task_t<std::function<void()>>;
    using mutex_t = std::mutex;
    using status_t = status::enum_t;
    using priority_t = priority::base_t;
    using err_handler_t = std::function<void(const char *)>;

public:
    inline base_tpool() = default;
    inline virtual ~base_tpool() = default;

    /// @brief adds the task to the queue to process with default_priority,
    ///
    /// @details wraps the invokable using pacakaged_task,
    /// adds to the queue to process and
    /// returns the future
    template <typename Func, typename... Args>
    inline auto add(Func &&func, Args &&...args)
        -> std::future<typename std::result_of<Func(Args...)>::type>;

    /// @brief adds the task to the queue to process with priority,
    /// tasks are evaluated based on their priority (behaviour can be overridden)
    ///
    /// @details wraps the invokable using pacakaged_task,
    /// adds to the queue to process and
    /// returns the future
    template <typename Func, typename... Args>
    inline auto add(priority_t priority, Func &&func, Args &&...args)
        -> std::future<typename std::result_of<Func(Args...)>::type>;

    /// wait for all tasks to complete
    inline void wait() const TUTIL_NOEXCEPT;

    /// wait for all tasks to complete if completed within the given time interval
    /// no effect of time interval is 0
    template <typename Rep, typename Period>
    inline void wait_for(const std::chrono::duration<Rep, Period> &time) const TUTIL_NOEXCEPT;

    /// wait for all tasks to complete if completed before the given time
    /// no effect of time is now() or past now()
    template <typename Clock, typename Duration>
    inline void wait_until(const std::chrono::time_point<Clock, Duration> &time) const TUTIL_NOEXCEPT;

    /// pauses the tpool
    /// consecutive calls will have no effect
    inline void pause() const TUTIL_NOEXCEPT;

    /// resumes the tpool
    /// consecutive calls will have no effect
    inline void resume() const TUTIL_NOEXCEPT;

    /// status of tpool
    inline status_t status() const TUTIL_NOEXCEPT;

    inline bool is_idle() const TUTIL_NOEXCEPT;    /// status() == status_t::idle ?
    inline bool is_running() const TUTIL_NOEXCEPT; /// status() == status_t::running ?
    inline bool is_paused() const TUTIL_NOEXCEPT;  /// status() == status_t::paused ?
    inline bool is_closed() const TUTIL_NOEXCEPT;  /// status() == status_t::closed ?

    /// thread count
    inline size_t size() const TUTIL_NOEXCEPT;

    /// thread count working on a task
    inline size_t count_working() const TUTIL_NOEXCEPT;

    /// idle thread count
    inline size_t count_idle() const TUTIL_NOEXCEPT;

    /// count of pending tasks
    inline size_t pending_tasks() const TUTIL_NOEXCEPT;

    /// set err_handler - thread safe
    inline void err_handler(err_handler_t err_handler_in) TUTIL_NOEXCEPT;

    /// get err_handler - thread safe
    inline err_handler_t err_handler() const TUTIL_NOEXCEPT;

protected:
    /// passed to threads on creation, to keep threads working
    inline virtual void tinit_() = 0;

    /// adds the task to the task container (tasks_)
    /// @note task must be moved
    /// @note mutex (task_mutex_) already locked
    inline virtual bool add_(task_t task);

    /// removes the task from the task container (tasks_) and returns it
    /// @note task must be moved
    /// @note mutex (task_mutex_) already locked
    inline virtual task_t get_();

    /// this call invokes the err handler (err_handler_)
    inline virtual void invoke_err_handler_(const char *msg) const;

    /// function call to check if the tasks are completed
    /// used by wait(), wait_for(), wait_until() and tinit_()
    inline bool waitfunc_() const TUTIL_NOEXCEPT;

protected:
    thread_container_t<std::thread> threads_;       // list of threads
    task_container_t<task_t> tasks_;                // list of pending tasks
    std::condition_variable task_condition_;        // condition var to wait on (for threads)
    std::condition_variable wait_condition_;        // condition var to support wait(), wait_for() and wait_until()
    mutable mutex_t task_mutex_;                    // mutex to synchronize
    std::atomic<status_t> status_ = status_t::idle; // status of tpool
    std::atomic<size_t> working_count_ = 0;         // count of threads executing a task
    err_handler_t err_handler_;                     // ptr to pass errors to (default set)
};

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
template <typename Func, typename... Args>
inline auto base_tpool<ThreadContainer, TaskContainer>::add(Func &&func, Args &&...args)
    -> std::future<typename std::result_of<Func(Args...)>::type>
{
    return add(priority::default_v, std::forward<Func>(func), std::forward<Args>(args)...);
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
template <typename Func, typename... Args>
inline auto base_tpool<ThreadContainer, TaskContainer>::add(priority_t priority, Func &&func, Args &&...args)
    -> std::future<typename std::result_of<Func(Args...)>::type>
{
    using return_t = typename std::result_of<Func(Args...)>::type;

    auto task = lambda_mover_t<std::packaged_task<return_t()>>(
        std::packaged_task<return_t()>(std::bind(
            std::forward<Func>(func), std::forward<Args>(args)...)));

    std::future<return_t> return_v = task.value.get_future();
    {
        std::unique_lock<mutex_t> lock(task_mutex_);

        if (is_closed())
        {
            invoke_err_handler_("cannot add task to a closed thread_pool");
            return std::future<return_t>();
        }

        auto task_toadd = task_t(priority, [task]()
                                 { task.value(); });
        if (!add_(std::move(task_toadd)))
        {
            invoke_err_handler_("failed adding task");
            return std::future<return_t>();
        }
    }
    task_condition_.notify_one();
    return return_v;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::add_(task_t task)
{
    tasks_.push_back(std::move(task));
    return true;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline typename base_tpool<ThreadContainer, TaskContainer>::task_t
base_tpool<ThreadContainer, TaskContainer>::get_()
{
    task_t task = std::move(tasks_.front());
    tasks_.pop_front();
    return std::move(task);
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::waitfunc_() const TUTIL_NOEXCEPT
{
    // the task_mutex is already locked, no need to lock it here
    return tasks_.empty() && working_count_ == 0;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline void base_tpool<ThreadContainer, TaskContainer>::wait() const TUTIL_NOEXCEPT
{
    std::unique_lock<mutex_t> lock(task_mutex_);
    wait_condition_.wait(lock, waitfunc_);
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
template <typename Rep, typename Period>
inline void base_tpool<ThreadContainer, TaskContainer>::wait_for(
    const std::chrono::duration<Rep, Period> &time) const TUTIL_NOEXCEPT
{
    std::unique_lock<mutex_t> lock(task_mutex_);
    wait_condition_.wait_for(lock, time, waitfunc_);
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
template <typename Clock, typename Duration>
inline void base_tpool<ThreadContainer, TaskContainer>::wait_until(
    const std::chrono::time_point<Clock, Duration> &time) const TUTIL_NOEXCEPT
{
    std::unique_lock<mutex_t> lock(task_mutex_);
    wait_condition_.wait_until(lock, time, waitfunc_);
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline void base_tpool<ThreadContainer, TaskContainer>::pause() const TUTIL_NOEXCEPT
{
    status_ = status_t::paused;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline void base_tpool<ThreadContainer, TaskContainer>::resume() const TUTIL_NOEXCEPT
{
    auto pending_tasks = pending_tasks();

    if (pending_tasks > 0)
    {
        status_ = status_t::running;
        task_condition_.notify_all();
    }
    else
    {
        status_ = status_t::idle;
    }
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline typename base_tpool<ThreadContainer, TaskContainer>::status_t
base_tpool<ThreadContainer, TaskContainer>::status() const TUTIL_NOEXCEPT
{
    return status_;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::is_idle() const TUTIL_NOEXCEPT
{
    return status() == status_t::idle;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::is_running() const TUTIL_NOEXCEPT
{
    return status() == status_t::running;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::is_paused() const TUTIL_NOEXCEPT
{
    return status() == status_t::paused;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::is_closed() const TUTIL_NOEXCEPT
{
    return status() == status_t::closed;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline size_t base_tpool<ThreadContainer, TaskContainer>::size() const TUTIL_NOEXCEPT
{
    return threads_.size();
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline size_t base_tpool<ThreadContainer, TaskContainer>::count_working() const TUTIL_NOEXCEPT
{
    return working_count_;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline size_t base_tpool<ThreadContainer, TaskContainer>::count_idle() const TUTIL_NOEXCEPT
{
    return size() - count_working();
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline size_t base_tpool<ThreadContainer, TaskContainer>::pending_tasks() const TUTIL_NOEXCEPT
{
    std::lock_guard<mutex_t> lock(task_mutex_);
    return tasks_.size();
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline void base_tpool<ThreadContainer, TaskContainer>::err_handler(err_handler_t err_handler_in) TUTIL_NOEXCEPT
{
    std::lock_guard<mutex_t> lock_(task_mutex_);
    err_handler_ = err_handler_in;
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline typename base_tpool<ThreadContainer, TaskContainer>::err_handler_t
base_tpool<ThreadContainer, TaskContainer>::err_handler() const TUTIL_NOEXCEPT
{
    std::lock_guard<mutex_t> lock_(task_mutex_);
    return err_handler_();
}

template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline void base_tpool<ThreadContainer, TaskContainer>::invoke_err_handler_(const char *msg) const
{
    if (err_handler_)
    {
        err_handler_(msg);
    }
}

TUTIL_NAMESPACE_END_MAIN