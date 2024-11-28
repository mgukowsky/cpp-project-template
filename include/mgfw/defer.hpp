#pragma once

#include <concepts>
#include <utility>

namespace mgfw {

/**
 * Creates an object wrapping a function, which shall be executed when the object goes out of scope.
 *
 * Inspired by `defer` in golang.
 */
template<typename T>
requires std::invocable<T>
class defer {
public:
  [[nodiscard("defer<T> must be assigned to a variable, otherwise it may execute "
              "immediately")]] explicit defer(T &&deferred)
    : deferred_(std::move(deferred)) { }

  ~defer() { deferred_(); }

  defer(const defer &)            = delete;
  defer &operator=(const defer &) = delete;
  defer(defer &&)                 = delete;
  defer &operator=(defer &&)      = delete;

private:
  T deferred_;
};

}  // namespace mgfw
