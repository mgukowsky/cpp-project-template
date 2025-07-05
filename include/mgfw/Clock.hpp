#pragma once

#include "mgfw/IClock.hpp"

namespace mgfw {

class Clock : public IClock {
public:
  ~Clock() override = default;

  TimePoint_t now() const noexcept override;

  void sleep_until(const TimePoint_t then) override;
};

}  // namespace mgfw
