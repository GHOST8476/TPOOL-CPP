#pragma once

#include "tutil/details/common.hpp"
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
    using thread_container_t = ThreadContainer<T>;
    template <typename T>
    using task_container_t = TaskContainer<T>;

public:
    using task_t = priority::task_t<std::function<void()>>;
    using mutex_t = std::mutex;
    using status_t = tpool_status::enum_t;
    using priority_t = priority::base_t;
    using err_handler_t = std::function<void(const char *)>;

public:
    inline base_tpool() = default;
    inline virtual ~base_tpool() = default;

    /// @brief adds the task to the queue to process,
    ///
    /// @details wraps the invokable using pacakaged_task,
    /// adds to the queue to process and
    /// returns the future
    template <typename Func, typename... Args>
    inline auto add(Func &&func, Args &&...args)
        -> std::future<typename std::result_of<Func(Args...)>::type>;

    /// @brief adds the task to the queue to process,
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
    /// consicutive calls will have no effect
    inline void pause() const TUTIL_NOEXCEPT;

    /// resumes the tpool
    /// consicutive calls will have no effect
    inline void resume() const TUTIL_NOEXCEPT;

    /// status of tpool
    inline status_t status() const TUTIL_NOEXCEPT;

    inline bool is_idle() const TUTIL_NOEXCEPT;
    inline bool is_running() const TUTIL_NOEXCEPT;
    inline bool is_paused() const TUTIL_NOEXCEPT;
    inline bool is_closed() const TUTIL_NOEXCEPT;

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
    inline virtual void tinit_() = 0;
    inline virtual bool add_(task_t task);
    inline virtual task_t get_();
    inline virtual void invoke_err_handler_(const char *msg) const;
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

    auto task = details::mover<std::packaged_task<return_t()>>(
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

        auto task_toadd = task_t(priority, [task]() mutable
                                 { task.value(); });
        if (add_(std::move(task_toadd)))
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
    tasks_.push_back(task);
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

// lock the task_mutex before calling this
template <template <typename> typename ThreadContainer,
          template <typename> typename TaskContainer>
inline bool base_tpool<ThreadContainer, TaskContainer>::waitfunc_() const TUTIL_NOEXCEPT
{
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

/// movable version of base_tpool
template <typename Impl, template <typename> typename Allocator = details::allocator>
class base_tpool_mv
{
    using this_t = base_tpool_mv<Impl, Allocator>;

protected:
    using impl_t = Impl;
    using alloc_t = Allocator<impl_t>;

public:
    using task_t = typename impl_t::task_t;
    using mutex_t = typename impl_t::mutex_t;
    using status_t = typename impl_t::status_t;
    using priority_t = typename impl_t::priority_t;
    using err_handler_t = typename impl_t::err_handler_t;

public:
    /// creates non resizable thread pool using stack memory
    template <typename... Args>
    base_tpool_mv(Args... args)
    {
        impl_t *pimpl_tmp = alloc_.allocate(1);
        if (pimpl_tmp == nullptr)
        {
            // err_handler is not set yet,
            // throw exceptions directly
            throw std::bad_alloc();
        }

        new (pimpl_tmp) impl_t(std::forward<Args>(args)...);
        pimpl_ = pimpl_tmp;
    }

    /// moves the tpool without blocking the thread
    /// @note tpool must be of same size
    template <template <typename> typename OtherAllocator>
    base_tpool_mv(base_tpool_mv<Impl, OtherAllocator> &&other)
        : pimpl_{other.pimpl_.exchange(nullptr)} {}

    /// moves the tpool without blocking the thread
    /// @note tpool must be of same size
    template <template <typename> typename OtherAllocator>
    this_t &operator=(base_tpool_mv<Impl, OtherAllocator> &&other)
    {
        pimpl_ = other.pimpl_.exchange(nullptr);
        return *this;
    }

    /// completes all tasks and joins all threads
    ~base_tpool_mv()
    {
        impl_t *pimpl_tmp = pimpl_;
        if (pimpl_tmp != nullptr)
        {
            (*pimpl_tmp).~impl_t();
            alloc_.deallocate(pimpl_tmp, 1);
            pimpl_ = nullptr;
        }
    }

    /// \brief adds the task to the queue to process,
    ///
    ///
    /// wraps the invokable using pacakaged_task,
    /// adds to the queue to process and
    /// returns the future
    template <typename Func, typename... Args>
    auto add(Func &&func, Args &&...args)
        -> decltype(std::declval<impl_t>().add(std::forward<Func>(func), std::forward<Args>(args)...))
    {
        return validate().add(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    /// \brief adds the task to the queue to process,
    ///
    ///
    /// wraps the invokable using pacakaged_task,
    /// adds to the queue to process and
    /// returns the future
    template <typename Func, typename... Args>
    auto add(priority_t priority, Func &&func, Args &&...args)
        -> decltype(std::declval<impl_t>().add(priority, std::forward<Func>(func), std::forward<Args>(args)...))
    {
        return validate().add(priority, std::forward<Func>(func), std::forward<Args>(args)...);
    }

    /// wait for all tasks to complete
    void wait() const TUTIL_NOEXCEPT
    {
        return validate().wait();
    }

    /// wait for all tasks to complete if completed within the given time interval
    /// no effect of time interval is 0
    template <typename Rep, typename Period>
    void wait_for(const std::chrono::duration<Rep, Period> &time) const TUTIL_NOEXCEPT
    {
        return validate().wait_for(time);
    }

    /// wait for all tasks to complete if completed before the given time
    /// no effect of time is now() or past now()
    template <typename Clock, typename Duration>
    void wait_until(const std::chrono::time_point<Clock, Duration> &time) const TUTIL_NOEXCEPT
    {
        return validate().wait_until(time);
    }

    /// pauses the tpool
    /// consicutive calls will have no effect
    void pause() const TUTIL_NOEXCEPT
    {
        return validate().pause();
    }

    /// resumes the tpool
    /// consicutive calls will have no effect
    void resume() const TUTIL_NOEXCEPT
    {
        return validate().resume();
    }

    /// status of tpool
    status_t status() const TUTIL_NOEXCEPT
    {
        return validate().status();
    }

    bool is_idle() const TUTIL_NOEXCEPT
    {
        return validate().is_idle();
    }

    bool is_running() const TUTIL_NOEXCEPT
    {
        return validate().is_running();
    }

    bool is_paused() const TUTIL_NOEXCEPT
    {
        return validate().is_paused();
    }

    bool is_closed() const TUTIL_NOEXCEPT
    {
        return validate().is_closed();
    }

    /// thread count
    size_t size() const TUTIL_NOEXCEPT
    {
        return validate().size();
    }

    /// thread count working on a task
    size_t count_working() const TUTIL_NOEXCEPT
    {
        return validate().count_working();
    }

    /// idle thread count
    size_t count_idle() const TUTIL_NOEXCEPT
    {
        return validate().count_idle();
    }

    /// count of pending tasks
    size_t pending_tasks() const TUTIL_NOEXCEPT
    {
        return validate().pending_tasks();
    }

    /// set err_handler - thread safe
    void err_handler(err_handler_t err_handler_in) TUTIL_NOEXCEPT
    {
        return validate().err_handler();
    }

    /// get err_handler - thread safe
    err_handler_t err_handler() const TUTIL_NOEXCEPT
    {
        return validate().err_handler();
    }

protected:
    impl_t &validate() const
    {
        impl_t *ptr = pimpl_;
        if (ptr == nullptr)
        {
            throw std::runtime_error("tpool is closed");
        }

        return *ptr;
    }

protected:
    // data is alloacted on heap to keep the tpool movable
    // atomic to keep move opertations thread safe
    alloc_t alloc_;
    std::atomic<impl_t *> pimpl_;
    // impl_t *pimpl_;
};

TUTIL_NAMESPACE_END_MAIN