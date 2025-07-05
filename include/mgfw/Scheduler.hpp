#pragma once

#include "mgfw/IClock.hpp"
#include "mgfw/ILogger.hpp"
#include "mgfw/types.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>

namespace mgfw {

/**
 * Simple timer queue-style scheduler.
 *
 * Accepts an optional clock source.
 */
class Scheduler {
public:
  using JobHandle_t = int;
  using JobFunc_t   = std::function<void()>;

  Scheduler(IClock &clock, ILogger &logger);
  ~Scheduler() = default;

  // In class declaration
  Scheduler(const Scheduler &)            = delete;
  Scheduler &operator=(const Scheduler &) = delete;

  Scheduler(Scheduler &&other) noexcept;
  Scheduler &operator=(Scheduler &&other) noexcept;

  /**
   * Stop a job by its ID. No effect if the job doesn't exist.
   */
  void cancel_job(const JobHandle_t jobId);

  /**
   * Run a job as soon as possible.
   *
   * Technically, adds a job with a deadline equal to when this function is invoked.
   */
  JobHandle_t do_now(JobFunc_t func, std::string desc = "");

  /**
   * Run a job on a recurring interval. Same idea as the JS API.
   */
  JobHandle_t set_interval(const Duration_t delay, JobFunc_t func, std::string desc = "");

  /**
   * Run a one-off job after a certain amount of time. Same idea as the JS API.
   */
  JobHandle_t set_timeout(const Duration_t delay, JobFunc_t func, std::string desc = "");

  /**
   * Start the scheduler. Will **NOT** return until `stop()` is called.
   */
  void run();

  /**
   * Stop the scheduler. No effect if the scheduler is not running.
   */
  void stop();

private:
  struct Job_ {
    const JobHandle_t id;
    TimePoint_t       deadline;
    Duration_t        interval;  // 0 if not repeating
    JobFunc_t         func;
    std::string       desc;

    bool operator>(const Job_ &other) const noexcept { return deadline > other.deadline; }
  };

  class JobComparator_ {
  public:
    bool operator()(const Job_ &a, const Job_ &b) const noexcept { return a > b; }
  };

  JobHandle_t schedule_(const Duration_t delay,
                        JobFunc_t      &&func,
                        const bool       repeat,
                        std::string    &&desc);

  IClock &clock_;

  ILogger                 &logger_;
  std::atomic_bool         running_;
  std::atomic<JobHandle_t> nextId_;

  // N.B. we're leveraging the properties of a std::set to use it as a priority queue. We can't
  // use std::priority_queue b/c we need random access to cancel jobs.
  std::set<Job_, JobComparator_> jobQueue_;
  std::mutex                     mtx_;
  std::condition_variable        cv_;
};

}  // namespace mgfw
