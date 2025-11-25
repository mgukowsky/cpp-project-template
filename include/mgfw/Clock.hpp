#pragma once

#include "mgfw/IClock.hpp"

namespace mgfw {

class Clock : public IClock {
public:
  ~Clock() override = default;

  Clock(const Clock &)            = default;
  Clock &operator=(const Clock &) = default;
  Clock(Clock &&)                 = default;
  Clock &operator=(Clock &&)      = default;

  TimePoint_t now() const noexcept override;

  void sleep_until(const TimePoint_t then) override;
};

}  // namespace mgfw
