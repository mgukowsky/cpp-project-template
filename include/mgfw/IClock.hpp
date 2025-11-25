#pragma once

#include "mgfw/types.hpp"

namespace mgfw {

class IClock {
public:
  IClock()                          = default;
  virtual ~IClock()                 = default;
  IClock(const IClock &)            = default;
  IClock(IClock &&)                 = default;
  IClock &operator=(const IClock &) = default;
  IClock &operator=(IClock &&)      = default;

  virtual TimePoint_t now() const noexcept = 0;

  virtual void sleep_until(const TimePoint_t then) = 0;
};

}  // namespace mgfw
