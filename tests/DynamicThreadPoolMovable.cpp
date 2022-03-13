#include <tutil/dtpool>
#include <gtest/gtest.h>

TEST(DynamicThreadPoolMovableTest, BasicTest)
{
    tutil::dtpool_mv<> movedtpool(5);
    tutil::dtpool_mv<> tpool = std::move(movedtpool);

    ASSERT_EQ(tpool.size(), 5);
    ASSERT_EQ(tpool.pending_tasks(), 0);
    // ASSERT_EQ(tpool.count_idle(), tpool.size());
    ASSERT_EQ(tpool.count_working(), 0);

    auto task_01 = [](int) -> void {};
    auto task_02 = [](char) -> void {};

    tpool.add(task_01, 0);
    tpool.add(task_02, 'a');
    tpool.add(task_02, 41);
}