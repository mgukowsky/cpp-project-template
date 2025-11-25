#pragma once

#include "mgfw/IClock.hpp"
#include "mgfw/types.hpp"

namespace mgfw_test {

/**
 * Alternate clock source that allows for deterministic testing of real-time code
 */
class ClockMock : public mgfw::IClock {
public:
  explicit ClockMock(const mgfw::TimePoint_t initialTime) : now_(initialTime) { }

  ~ClockMock() override                   = default;
  ClockMock(const ClockMock &)            = delete;
  ClockMock(ClockMock &&)                 = delete;
  ClockMock &operator=(const ClockMock &) = delete;
  ClockMock &operator=(ClockMock &&)      = delete;

  mgfw::TimePoint_t now() const noexcept override { return now_; }

  void set_now(const mgfw::TimePoint_t now) noexcept { now_ = now; }

  void sleep_until([[maybe_unused]] const mgfw::TimePoint_t then) override { /* noop */ }

private:
  mgfw::TimePoint_t now_;
};

}  // namespace mgfw_test
