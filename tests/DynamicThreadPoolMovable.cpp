#include <tutil/dynamic>
#include <gtest/gtest.h>

TEST(DynamicThreadPoolMovableTest, BasicTest)
{
    tutil::dynamic_mv<> movedynamic(5);
    tutil::dynamic_mv<> tpool = std::move(movedynamic);

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