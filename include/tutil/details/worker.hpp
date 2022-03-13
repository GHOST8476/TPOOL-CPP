#pragma once
#include "tutil/details/ftpool.hpp"

TUTIL_NAMESPACE_BEGIN_MAIN

/// reusable worker thread
/// tasks are evaluated based on their priority
template <template <typename> typename TaskQueue = details::deque>
class worker : private ftpool<1, TaskQueue>
{
    using base_t = ftpool<1, TaskQueue>;

public:
    using task_t = typename base_t::task_t;
    using mutex_t = typename base_t::mutex_t;
    using status_t = typename base_t::status_t;
    using priority_t = typename base_t::priority_t;
    using err_handler_t = typename base_t::err_handler_t;

public:
    using base_t::add;
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
};

/// movable version of worker
template <template <typename> typename TaskQueue = details::deque,
          template <typename> typename Allocator = details::allocator>
class worker_mv : private ftpool_mv<1, TaskQueue, Allocator>
{
    using base_t = ftpool_mv<1, TaskQueue, Allocator>;

public:
    using task_t = typename base_t::task_t;
    using mutex_t = typename base_t::mutex_t;
    using status_t = typename base_t::status_t;
    using priority_t = typename base_t::priority_t;
    using err_handler_t = typename base_t::err_handler_t;

public:
    using base_t::add;
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
};

TUTIL_NAMESPACE_END_MAIN