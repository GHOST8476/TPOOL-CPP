#pragma once

////////////////////////////////////////////////////////////////////////////////////////
/// NAMESPACES
////////////////////////////////////////////////////////////////////////////////////////
#define TUTIL_NAMESPACE_BEGIN_MAIN \
    namespace tpool                \
    {
#define TUTIL_NAMESPACE_END_MAIN }

////////////////////////////////////////////////////////////////////////////////////////
/// SPECIFIERS
////////////////////////////////////////////////////////////////////////////////////////

// visual studio up to 2013 does not support noexcept nor constexpr
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define TUTIL_NOEXCEPT _NOEXCEPT
#define TUTIL_CONSTEXPR
#else
#define TUTIL_NOEXCEPT noexcept
#define TUTIL_CONSTEXPR constexpr
#endif

////////////////////////////////////////////////////////////////////////////////////////
/// Common Details
////////////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <deque>
#include <memory>
#include <vector>

TUTIL_NAMESPACE_BEGIN_MAIN

// TODO: use this, looks cute
// ¯\_(ツ)_/¯

namespace details
{
    /// to move the object in lambda expressions
    template <typename T>
    struct lambda_mover
    {
        lambda_mover(T &value_in) : value{std::move(value_in)} {}
        lambda_mover(const lambda_mover &other) : value{std::move(other.value)} {}
        lambda_mover(lambda_mover &&other) : value{std::move(other.value)} {}
        ~lambda_mover() = default;

        mutable T value;
    };
}

namespace details
{
    template <size_t Size>
    struct array
    {
        template <typename T>
        using type = std::array<T, Size>;
    };

    template <typename T>
    using vector = std::vector<T>;

    template <typename T>
    using deque = std::deque<T>;

    template <typename T>
    using allocator = std::allocator<T>;
}

namespace priority
{
    using base_t = int;
    constexpr base_t default_v = 0;

    enum enum_t : base_t
    {
        low,
        med,
        high,
        realtime
    };

    constexpr std::array<const char *, 4> strings = {"low", "med", "high", "realtime"};
    constexpr const char *tostr(enum_t value) TUTIL_NOEXCEPT
    {
        return strings[value];
    }
}

namespace status
{
    using base_t = unsigned char;
    enum enum_t : base_t
    {
        idle,
        running,
        paused,
        closed
    };

    constexpr std::array<const char *, 4> strings = {"idle", "running", "paused", "closed"};
    constexpr const char *tostr(enum_t value) TUTIL_NOEXCEPT
    {
        return strings[value];
    }
}

template <typename Task>
struct task_t
{
private:
    using this_t = task_t<Task>;

public:
    using priority_t = priority::base_t;

public:
    task_t() = default;
    ~task_t() = default;

    // constructor
    task_t(priority_t priority_in, Task task_in)
        : priority{priority_in}, task{task_in} {}

    // copy constructor
    task_t(const this_t &other) = delete;

    // move constructor
    task_t(this_t &&other)
        : priority{other.priority},
          task{std::move(other.task)} {}

    // copy operator
    this_t &operator=(const this_t &other) = delete;

    // move operator
    this_t &operator=(this_t &&other)
    {
        priority = other.priority;
        task = std::move(other.task);

        return this;
    }

    // to use as the underlying task
    void operator()()
    {
        task();
    }

public:
    priority_t priority;
    Task task;
};

TUTIL_NAMESPACE_END_MAIN
