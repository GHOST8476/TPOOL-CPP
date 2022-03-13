#pragma once

////////////////////////////////////////////////////////////////////////////////////////
/// NAMESPACES
////////////////////////////////////////////////////////////////////////////////////////
#define TUTIL_NAMESPACE_BEGIN_MAIN \
    namespace tutil                \
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
    template <typename T>
    struct mover
    {
        mover(T &value_in) : value{std::move(value_in)} {}
        mover(const mover &other) : value{std::move(other.value)} {}
        mover(mover &&other) : value{std::move(other.value)} {}
        ~mover() = default;

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

    template <typename Task>
    struct task_t
    {
        task_t() = default;

        task_t(base_t priority_in, Task task_in)
            : priority{priority_in}, task{task_in} {}

        base_t priority;
        Task task;

        void operator()()
        {
            task();
        }
    };
}

namespace tpool_status
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
    const char *tostr(enum_t value)
    {
        return strings[value];
    }
}

TUTIL_NAMESPACE_END_MAIN
