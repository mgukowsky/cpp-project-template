#pragma once

#include "mgfw/IClock.hpp"
#include "mgfw/types.hpp"

#include <atomic>

namespace mgfw_test {

/**
 * Alternate clock source that allows for deterministic testing of real-time code
 */
class ClockMock : public mgfw::IClock {
public:
  explicit ClockMock(const mgfw::TimePoint_t initialTime)
    : now_(initialTime), barrier_(0), shouldBlock_(true) { }

  ~ClockMock() override                   = default;
  ClockMock(const ClockMock &)            = delete;
  ClockMock(ClockMock &&)                 = delete;
  ClockMock &operator=(const ClockMock &) = delete;
  ClockMock &operator=(ClockMock &&)      = delete;

  mgfw::TimePoint_t now() const noexcept override { return now_; }

  void set_now(const mgfw::TimePoint_t now) noexcept { now_ = now; }

  /**
   * If set to false, then calls to sleep_until() will return immediately.
   */
  void set_should_block(const bool shouldBlock) { shouldBlock_.store(shouldBlock); }

  /**
   * This specialization will block until wake_sleepers() is called, regardless of the time point
   * argument that is provided.
   *
   * Returns immediately if `should_block` is true.
   */
  void sleep_until([[maybe_unused]] const mgfw::TimePoint_t then) override {
    if(shouldBlock_.load()) {
      mgfw::U64 old = barrier_.load();
      barrier_.wait(old);
    }
  }

  /**
   * Unblocks threads that have called sleep_until, however may wait synchronously until the number
   * of sleepers specified in the constructor call sleep_until.
   *
   * May be called repeatedly to wait for successive waves of sleepers.
   */
  void wake_sleepers() {
    barrier_.fetch_add(1);
    barrier_.notify_all();
  }

private:
  mgfw::TimePoint_t now_;

  /**
   * If `shouldBlock_` is true, then threads will wait on this object when `wake_sleepers` is
   * called.
   */
  std::atomic_uint64_t barrier_;

  /**
   * If false, then `sleep_until` will return immediately.
   */
  std::atomic_bool shouldBlock_;
};

}  // namespace mgfw_test
