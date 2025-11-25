#include "mgfw/Scheduler.hpp"

#include "gmock/gmock.h"
#include "mgfw/IClock.hpp"
#include "mgfw/types.hpp"
#include "mgfw_test/ClockMock.hpp"
#include "mgfw_test/LoggerMock.hpp"

#include <bits/chrono.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <format>
#include <future>
#include <latch>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

using mgfw::Duration_t;
using mgfw::IClock;
using mgfw::Scheduler;
using mgfw::TimePoint_t;
using JobHandle_t = mgfw::Scheduler::JobHandle_t;

using mgfw_test::ClockMock;
using mgfw_test::LoggerMock;

using ::testing::HasSubstr;

namespace {

// Gross, but this lets us atomically set the clock using the same lock as the scheduler. Important
// b/c the CV's predicate accesses the clock!
template<typename T>
requires requires(T t) { TimePoint_t(t); }
void safe_set_clock(Scheduler &sched, const T clockVal) {
  sched.access_clock_sync([clockVal](IClock &schedClk) {
    dynamic_cast<ClockMock &>(schedClk).set_now(TimePoint_t(clockVal));
  });
}

}  // namespace

/**
 * N.B. the pattern we use to exit multithreaded tests: we block a job on the scheduler thread until
 * the main thread has called `request_stop()`. This avoids races.
 */

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
  std::promise<void> step2;
  std::atomic_bool   run_called{false};
  std::atomic_bool   cancel_called{false};

  sched.set_timeout(
    500ms,
    [&] {
      run_called = true;
      step1.set_value();
      step2.get_future().wait();
    },
    "job to run");

  // Earlier job, bue we're gonna skip it.
  auto id = sched.set_timeout(100ms, [&] { cancel_called = true; }, "job to cancel");
  sched.cancel_job(id);

  {
    const std::jthread t([&] { sched.run(); });

    safe_set_clock(sched, 500ms);
    sched.get_cv().notify_all();
    step1.get_future().wait();

    sched.request_stop();
    step2.set_value();
  }

  EXPECT_TRUE(run_called);
  EXPECT_FALSE(cancel_called);
}

TEST(SchedulerTest, MultipleJobsExecuteInOrder) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> finished;
  std::vector<int>   order;
  std::latch         allJobDoneLatch(3);

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
      finished.get_future().wait();
    },
    "second");
  sched.set_timeout(
    150ms,
    [&] {
      order.push_back(3);
      allJobDoneLatch.count_down();
    },
    "third");

  safe_set_clock(sched, 500ms);

  {
    const std::jthread t([&] { sched.run(); });

    allJobDoneLatch.wait();

    sched.request_stop();
    finished.set_value();
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
      sched.request_stop();
    },
    "third");

  safe_set_clock(sched, 500ms);
  sched.run();

  ASSERT_EQ(order.size(), 2);
  EXPECT_EQ(order[0], 1);
  EXPECT_EQ(order[1], 3);
}

TEST(SchedulerTest, DoNowRunsImmediately) {
  ClockMock  clk(TimePoint_t(0ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::vector<int> const order;

  const int MAGIC = 42;
  int       i     = 0;

  sched.do_now([&] {
    i = MAGIC;
    sched.request_stop();
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
  std::promise<void> step3;
  std::promise<void> step4;

  bool onStep3 = false;

  sched.do_now([&] {
    step1.set_value();
    step2.get_future().wait();
  });
  sched.set_timeout(100ms, [&] {
    onStep3 = true;
    step3.set_value();
    step4.get_future().wait();
  });

  {
    std::jthread const t([&] { sched.run(); });

    step1.get_future().wait();
    sched.request_stop();
    step2.set_value();
  }
  // Timeout job should not have run yet
  EXPECT_FALSE(onStep3);

  safe_set_clock(sched, 151ms);

  std::jthread const t([&] { sched.run(); });
  step3.get_future().wait();

  EXPECT_TRUE(onStep3);
  sched.request_stop();
  step4.set_value();
}

TEST(SchedulerTest, DoNowExecutesImmediately) {
  ClockMock  clk(TimePoint_t(50ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;
  std::promise<void> step2;

  bool onStep1 = false;

  sched.do_now([&] {
    onStep1 = true;
    step1.set_value();
    step2.get_future().wait();
  });

  std::jthread const t([&] { sched.run(); });

  step1.get_future().wait();

  // Timeout job should not run yet
  EXPECT_TRUE(onStep1);

  sched.request_stop();
  step2.set_value();
}

/**
 * The locking order in this test was a pain in the ass to get right, but it avoids race conditions.
 *
 * Having the recurring job wait on the future ensures that the next iteration is scheduled before
 * we set the clock. If we don't do this, then a race like the following is possible:
 *
 *      T1(main)                    T2(sched)
 *                                  - set promise
 *      - unblock wait on promise
 *      - clock is set to 200
 *                                  - job exits
 *                                  - next iteration is scheduled:
 *                                  - next iteration dealine = **250**
 *                                                              ^ normally next dealine would be
 *                                                                150 + 50 = 200, but b/c that
 *                                                                would already be expired, the
 *                                                                new deadline = clock.now() + 50
 *                                  - sched waits on CV forever b/c the next deadline will always
 *                                    be > clock.now()
 *      - wait on next promise
 *        forever b/c sched
 *        will never set it
 *
 * By using do_now() to insert an job that blocks the main thread, we can guarantee that the next
 * iteration will be scheduled correctly before we advance the clock!
 *
 */
TEST(SchedulerTest, SetIntervalExecutesRepeatedly) {
  ClockMock  clk(TimePoint_t(100ms));
  LoggerMock log;
  Scheduler  sched(clk, log);

  std::promise<void> step1;
  std::promise<void> step2;
  std::promise<void> step3;
  std::promise<void> step4;
  std::promise<void> step5;
  std::promise<void> step6;
  std::atomic_int    call_count{0};

  sched.set_interval(
    50ms,
    [&] {
      int const cnt = call_count.fetch_add(1);
      switch(cnt) {
        case 0:
          sched.do_now([&] {
            step1.set_value();
            step2.get_future().wait();
          });
          break;
        case 1:
          sched.do_now([&] {
            step3.set_value();
            step4.get_future().wait();
          });
          break;
        case 2:
          sched.do_now([&] {
            step5.set_value();
            step6.get_future().wait();
          });
          break;
        default:
          break;
      }
    },
    "interval job");

  {
    safe_set_clock(sched, 150ms);

    std::jthread const t([&] { sched.run(); });
    step1.get_future().wait();

    safe_set_clock(sched, 200ms);
    step2.set_value();
    step3.get_future().wait();

    safe_set_clock(sched, 250ms);
    step4.set_value();
    step5.get_future().wait();

    sched.request_stop();
    step6.set_value();
  }
  EXPECT_EQ(call_count.load(), 3);
}
