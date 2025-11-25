#include "mgfw/Scheduler.hpp"

#include "mgfw/IClock.hpp"
#include "mgfw/ILogger.hpp"
#include "mgfw/types.hpp"

#include <algorithm>
#include <condition_variable>
#include <format>
#include <functional>
#include <string>
#include <utility>

namespace mgfw {

Scheduler::Scheduler(IClock &clock, ILogger &logger)
  : logger_(logger), syncState_(clock, false, 1, JobQueue_t_(&Scheduler::job_comparator_)) { }

Scheduler::~Scheduler() { request_stop(); }

Scheduler::Scheduler(Scheduler &&other) noexcept
  : logger_(other.logger_),
    // SyncCell has atomic move semantics
    syncState_(std::move(other.syncState_)) { }  // N.B. cv is default-constructed

Scheduler &Scheduler::operator=(Scheduler &&other) noexcept {
  if(this != &other) {
    logger_ = other.logger_;

    auto thisState  = syncState_.get_locked();
    auto otherState = other.syncState_.get_locked();

    // We assign the clock reference...
    thisState->clock_ = otherState->clock_;

    // ...but move the rest
    thisState->running_  = otherState->running_;  // trivial type, so no move needed
    thisState->nextId_   = otherState->nextId_;   // ditto
    thisState->jobQueue_ = std::move(otherState->jobQueue_);

    // cv_ needs to be left as-is
  }
  return *this;
}

void Scheduler::access_clock_sync(const std::function<void(IClock &)> &fn) {
  auto syncState = syncState_.get_locked();
  fn(syncState->clock_);
}

void Scheduler::cancel_job(const JobHandle_t jobId) {
  auto syncState = syncState_.get_locked();

  auto it =
    std::ranges::find_if(syncState->jobQueue_, [&](const Job_ &job) { return job.id == jobId; });

  if(it == syncState->jobQueue_.end()) {
    logger_.error(std::format("No job found with ID {}", jobId));
  }
  else {
    syncState->jobQueue_.erase(it);
  }
}

Scheduler::JobHandle_t Scheduler::do_now(JobFunc_t func, std::string desc) {
  return schedule_(Duration_t{0}, std::move(func), false, std::move(desc));
}

std::condition_variable &Scheduler::get_cv() { return cv_; }

Scheduler::JobHandle_t Scheduler::set_interval(const Duration_t delay,
                                               JobFunc_t        func,
                                               std::string      desc) {
  return schedule_(delay, std::move(func), true, std::move(desc));
}

Scheduler::JobHandle_t Scheduler::set_timeout(const Duration_t delay,
                                              JobFunc_t        func,
                                              std::string      desc) {
  return schedule_(delay, std::move(func), false, std::move(desc));
}

void Scheduler::run() {
  {
    auto syncState      = syncState_.get_locked();
    syncState->running_ = true;
  }

  while(true) {
    bool isEmpty = true;

    {
      auto syncState = syncState_.get_locked();
      if(!syncState->running_) {
        break;
      }
      isEmpty = syncState->jobQueue_.empty();
    }

    if(isEmpty) {
      syncState_.cv_wait(cv_, [](const SyncState &syncState) {
        return !syncState.running_ || !syncState.jobQueue_.empty();
      });
      continue;
    }

    TimePoint_t nextDeadline;
    {
      auto syncState = syncState_.get_locked();
      nextDeadline   = syncState->jobQueue_.begin()->deadline;
    }

    syncState_.cv_wait_until(cv_, nextDeadline, [](const SyncState &syncState) {
      return !syncState.running_
          || (!syncState.jobQueue_.empty()
              && syncState.jobQueue_.begin()->deadline > syncState.clock_.now());
    });

    // It's possible that a job earlier than `nextDeadline` was added while we were waiting, but
    // that's fine
    TimePoint_t now;
    while(true) {
      Job_ job;
      {
        auto syncState = syncState_.get_locked();

        now = syncState->clock_.now();

        if(syncState->jobQueue_.empty() || !syncState->running_
           || syncState->jobQueue_.begin()->deadline > now)
        {
          break;
        }

        auto it = syncState->jobQueue_.begin();
        job     = *it;
        syncState->jobQueue_.erase(it);
      }

      // N.B. we obviously don't hold the mutex while executing the job
      try {
        job.func();
      }
      catch(...) {
        logger_.error(std::format("Job {} ({}) threw an exception!", job.id, job.desc));
      }

      {
        auto syncState = syncState_.get_locked();
        // Account for any time that passed while we were running the job
        now = syncState->clock_.now();

        if(job.interval != Duration_t{0}) {
          job.deadline = job.deadline + job.interval;

          // If the next deadline is already expired, then we adjust and just make the next interval
          // relative to now. If, say, the clock jumps forward (e.g. the program is suspended) then
          // this prevents multiple expired deadlines from "piline up".
          if(job.deadline <= now) {
            job.deadline = now + job.interval;
          }

          syncState->jobQueue_.insert(job);
        }
      }
    }
  }
}

void Scheduler::request_stop() {
  auto syncState      = syncState_.get_locked();
  syncState->running_ = false;
  cv_.notify_all();
}

bool Scheduler::job_comparator_(const Job_ &lhs, const Job_ &rhs) noexcept {
  return lhs.deadline < rhs.deadline;
}

Scheduler::JobHandle_t Scheduler::schedule_(const Duration_t delay,
                                            JobFunc_t      &&func,
                                            const bool       repeat,
                                            std::string    &&desc) {
  auto syncState = syncState_.get_locked();

  const JobHandle_t jobHandle = syncState->nextId_;
  syncState->nextId_++;

  Job_ job{
    .id       = jobHandle,
    .deadline = syncState->clock_.now() + delay,
    .interval = repeat ? delay : Duration_t{0},
    .func     = std::move(func),
    .desc     = std::move(desc),
  };

  syncState->jobQueue_.emplace(std::move(job));
  cv_.notify_one();

  return jobHandle;
}

}  // namespace mgfw
