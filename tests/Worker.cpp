#include <tpool/worker>
#include <gtest/gtest.h>

TEST(WorkerTest, BasicTest)
{
    tpool::worker<> worker;

    ASSERT_EQ(worker.pending_tasks(), 0);

    auto task_01 = [](int) -> void {};
    auto task_02 = [](char) -> void {};

    worker.add(task_01, 0);
    worker.add(task_02, 'a');
    worker.add(task_02, 41);
}