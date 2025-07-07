#include "mgfw/Scheduler.hpp"

#include "mgfw/types.hpp"
#include "mgfw_test/ClockMock.hpp"
#include "mgfw_test/LoggerMock.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <future>
#include <latch>
#include <thread>

using namespace std::chrono_literals;

using mgfw::Duration_t;
using mgfw::Scheduler;
using mgfw::TimePoint_t;
using JobHandle_t = mgfw::Scheduler::JobHandle_t;

using mgfw_test::ClockMock;
using mgfw_test::LoggerMock;

using ::testing::HasSubstr;

TEST(SchedulerTest, CancelJobLogsErrorIfNotFound) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  const JobHandle_t jobId = 42;
  EXPECT_CALL(log, error(HasSubstr(std::format("No job found with ID {}", jobId))));
  sched.cancel_job(jobId);
}

TEST(SchedulerTest, CancelJobPreventsExecution) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;
  std::atomic_bool   run_called{false};
  std::atomic_bool   cancel_called{false};

  sched.set_timeout(
    500ms,
    [&] {
      run_called = true;
      step1.set_value();
    },
    "job to run");

  // Earlier job, bue we're gonna skip it.
  auto id = sched.set_timeout(100ms, [&] { cancel_called = true; }, "job to cancel");
  sched.cancel_job(id);

  {
    std::jthread t([&] { sched.run(); });

    clk.set_now(TimePoint_t(500ms));
    sched.get_cv().notify_all();
    step1.get_future().wait();

    sched.stop();
    sched.get_cv().notify_all();
  }

  EXPECT_TRUE(run_called);
  EXPECT_FALSE(cancel_called);
}

TEST(SchedulerTest, MultipleJobsExecuteInOrder) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::vector<int> order;
  std::latch       allJobDoneLatch(3);

  sched.set_timeout(
    100ms,
    [&] {
      order.push_back(1);
      allJobDoneLatch.count_down();
    },
    "first");
  sched.set_timeout(
    200ms,
    [&] {
      order.push_back(2);
      allJobDoneLatch.count_down();
    },
    "second");
  sched.set_timeout(
    150ms,
    [&] {
      order.push_back(3);
      allJobDoneLatch.count_down();
    },
    "third");

  clk.set_now(TimePoint_t(500ms));

  {
    std::jthread t([&] { sched.run(); });

    allJobDoneLatch.wait();

    sched.stop();
  }

  ASSERT_EQ(order.size(), 3);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 3);
  EXPECT_EQ(order[2], 2);
}

TEST(SchedulerTest, CanRunSingleThreaded) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::vector<int> order;

  sched.set_timeout(100ms, [&] { order.push_back(1); }, "first");
  sched.set_timeout(
    200ms,
    [&] {
      // N.B. that this won't run!!!
      order.push_back(2);
    },
    "second");
  sched.set_timeout(
    150ms,
    [&] {
      order.push_back(3);
      sched.stop();
    },
    "third");

  clk.set_now(TimePoint_t(500ms));
  sched.run();

  ASSERT_EQ(order.size(), 2);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 3);
}

TEST(SchedulerTest, DoNowRunsImmediately) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::vector<int> order;

  const int MAGIC = 42;
  int       i     = 0;

  sched.do_now([&] {
    i = MAGIC;
    sched.stop();
  });
  sched.run();

  EXPECT_EQ(MAGIC, i);
}

TEST(SchedulerTest, SetTimeoutExecutesAfterDelay) {
  ClockMock  clk(TimePoint_t(50ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;
  std::promise<void> step2;

  bool onStep2 = false;

  sched.do_now([&] { step1.set_value(); });
  sched.set_timeout(100ms, [&] {
    onStep2 = true;
    step2.set_value();
  });

  std::jthread t([&] { sched.run(); });

  step1.get_future().wait();

  // Timeout job should not run yet
  EXPECT_FALSE(onStep2);

  clk.set_now(TimePoint_t(151ms));
  sched.get_cv().notify_all();

  step2.get_future().wait();

  EXPECT_TRUE(onStep2);

  sched.stop();
  sched.get_cv().notify_all();
}

TEST(SchedulerTest, DoNowExecutesImmediately) {
  ClockMock  clk(TimePoint_t(50ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;

  bool onStep1 = false;

  sched.do_now([&] {
    onStep1 = true;
    step1.set_value();
  });

  std::jthread t([&] { sched.run(); });

  step1.get_future().wait();

  // Timeout job should not run yet
  EXPECT_TRUE(onStep1);

  sched.stop();
  sched.get_cv().notify_all();
}

TEST(SchedulerTest, SetIntervalExecutesRepeatedly) {
  ClockMock  clk(TimePoint_t(100ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;
  std::promise<void> step2;
  std::promise<void> step3;
  std::atomic_int    call_count{0};

  sched.set_interval(
    50ms,
    [&] {
      int cnt = call_count.fetch_add(1);
      switch(cnt) {
        case 0:
          step1.set_value();
          break;
        case 1:
          step2.set_value();
          break;
        case 2:
          step3.set_value();
          break;
        default:
          break;
      }
    },
    "interval job");

  std::promise<void> scheduler_started;

  {
    std::jthread t([&] { sched.run(); });

    // Trigger each run in turn
    clk.set_now(TimePoint_t(150ms));
    sched.get_cv().notify_all();
    step1.get_future().wait();

    clk.set_now(TimePoint_t(200ms));
    sched.get_cv().notify_all();
    step2.get_future().wait();

    clk.set_now(TimePoint_t(250ms));
    sched.get_cv().notify_all();
    step3.get_future().wait();

    sched.stop();
    sched.get_cv().notify_all();
  }
  EXPECT_EQ(call_count.load(), 3);
}
