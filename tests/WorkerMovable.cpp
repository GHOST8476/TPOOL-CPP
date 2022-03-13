#include <tutil/worker>
#include <gtest/gtest.h>

TEST(WorkerMovableTest, BasicTest)
{
    std::cout << std::endl;

    tutil::worker_mv<> moved_worker;
    tutil::worker_mv<> worker = std::move(moved_worker);

    ASSERT_EQ(worker.pending_tasks(), 0);

    auto task_01 = [](int) -> void {};
    auto task_02 = [](char) -> void {};

    worker.add(task_01, 0);
    worker.add(task_02, 'a');
    worker.add(task_02, 41);

    std::cout << std::endl;
}