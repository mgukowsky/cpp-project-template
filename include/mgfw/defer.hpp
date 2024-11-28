#pragma once

#include <concepts>

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
              "immediately")]] defer(T &&deferred)
    : deferred_(deferred) { }

  ~defer() { deferred_(); }

private:
  T deferred_;
};

}  // namespace mgfw
