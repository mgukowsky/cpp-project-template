#include "mgfw/Scheduler.hpp"

#include <algorithm>
#include <format>

namespace mgfw {

Scheduler::Scheduler(IClock &clock, ILogger &logger)
  : clock_(clock),
    logger_(logger),
    running_(false),
    nextId_(1),
    jobQueue_(&Scheduler::job_comparator_) { }

Scheduler::Scheduler(Scheduler &&other) noexcept
  : clock_(other.clock_),
    logger_(other.logger_),
    running_(other.running_.load()),
    nextId_(other.nextId_.load()),

    // A little hacky, but we need to lock other's job queue before moving it
    jobQueue_([&] {
      std::scoped_lock lck(other.mtx_);
      return std::move(other.jobQueue_);
    }()) {
  // Fresh mutex_ and cv_ are default-constructed
}

Scheduler &Scheduler::operator=(Scheduler &&other) noexcept {
  if(this != &other) {
    std::scoped_lock lock(mtx_, other.mtx_);
    clock_  = other.clock_;
    logger_ = other.logger_;
    running_.store(other.running_.load());
    nextId_.store(other.nextId_.load());
    jobQueue_ = std::move(other.jobQueue_);
    // mutex_ and cv_ are left default-initialized
  }
  return *this;
}

void Scheduler::cancel_job(const JobHandle_t jobId) {
  std::scoped_lock lck(mtx_);

  auto it = std::ranges::find_if(jobQueue_, [&](const Job_ &job) { return job.id == jobId; });

  if(it == jobQueue_.end()) {
    logger_.error(std::format("No job found with ID {}", jobId));
  }
  else {
    jobQueue_.erase(it);
  }
}

Scheduler::JobHandle_t Scheduler::do_now(JobFunc_t func, std::string desc) {
  std::scoped_lock lck(mtx_);

  return schedule_(Duration_t{0}, std::move(func), false, std::move(desc));
}

std::condition_variable &Scheduler::get_cv() { return cv_; }

Scheduler::JobHandle_t Scheduler::set_interval(const Duration_t delay,
                                               JobFunc_t        func,
                                               std::string      desc) {
  std::scoped_lock lck(mtx_);

  return schedule_(delay, std::move(func), true, std::move(desc));
}

Scheduler::JobHandle_t Scheduler::set_timeout(const Duration_t delay,
                                              JobFunc_t        func,
                                              std::string      desc) {
  std::scoped_lock lck(mtx_);

  return schedule_(delay, std::move(func), false, std::move(desc));
}

void Scheduler::run() {
  running_.store(true);

  while(running_.load()) {
    std::unique_lock lck(mtx_);

    if(jobQueue_.empty()) {
      cv_.wait(lck, [&] { return !running_.load() || !jobQueue_.empty(); });
      continue;
    }

    const auto nextDeadline = jobQueue_.begin()->deadline;

    cv_.wait_until(lck, nextDeadline, [&] {
      return !running_.load()
          || (!jobQueue_.empty() && jobQueue_.begin()->deadline <= clock_.now());
    });

    // It's possible that a job earlier than `nextDeadline` was added while we're waiting, but
    // that's fine
    TimePoint_t now = clock_.now();
    while(!jobQueue_.empty() && jobQueue_.begin()->deadline <= now && running_.load()) {
      auto it  = jobQueue_.begin();
      auto job = *it;
      jobQueue_.erase(it);

      lck.unlock();

      try {
        job.func();
      }
      catch(...) {
        logger_.error(std::format("Job {} ({}) threw an exception!", job.id, job.desc));
      }

      // We will stay locked through the check at the top of the while loop
      lck.lock();

      // Account for any time that passed while we were running the job
      now = clock_.now();

      if(job.interval != Duration_t{0}) {
        job.deadline = job.deadline + job.interval;
        jobQueue_.insert(job);

        // If the next deadline is already expired, then we adjust and just make the next interval
        // relative to now. If, say, the clock jumps forward (e.g. the program is suspended) then
        // this prevents multiple expired deadlines from "piline up".
        if(job.deadline <= now) {
          job.deadline = now + job.interval;
        }
      }
    }
  }
}

void Scheduler::stop() {
  running_.store(false);
  cv_.notify_all();
}

bool Scheduler::job_comparator_(const Job_ &lhs, const Job_ &rhs) noexcept {
  return lhs.deadline < rhs.deadline;
}

Scheduler::JobHandle_t Scheduler::schedule_(const Duration_t delay,
                                            JobFunc_t      &&func,
                                            const bool       repeat,
                                            std::string    &&desc) {
  const JobHandle_t jobHandle = nextId_.fetch_add(1);

  Job_ job{
    .id       = jobHandle,
    .deadline = clock_.now() + delay,
    .interval = repeat ? delay : Duration_t{0},
    .func     = std::move(func),
    .desc     = std::move(desc),
  };

  jobQueue_.emplace(std::move(job));
  cv_.notify_one();

  return jobHandle;
}

}  // namespace mgfw
