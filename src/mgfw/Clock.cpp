#include "mgfw/Clock.hpp"

#include "mgfw/types.hpp"

#include <chrono>
#include <thread>

namespace mgfw {

TimePoint_t Clock::now() const noexcept { return std::chrono::steady_clock::now(); }

void Clock::sleep_until(const TimePoint_t then) { std::this_thread::sleep_until(then); }

}  // namespace mgfw
