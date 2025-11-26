#pragma once

#include "mgfw/types.hpp"

#include <concepts>
#include <condition_variable>
#include <mutex>
#include <type_traits>
#include <utility>

namespace mgfw {

template<typename CV_t, typename Lock_t, typename Predicate_t>
concept is_condvar = requires(CV_t cv, Lock_t lck, Predicate_t pred, TimePoint_t time) {
  { cv.wait(lck, pred) } -> std::same_as<void>;
  { cv.wait_until(lck, time, pred) } -> std::same_as<bool>;
  /* Not using wait_for, so not checking it */
};

/**
 * Simple Rust-style mutex; i.e. the scoped lock itself is used to atomically access the object
 */
template<typename T, typename Lock_t = std::mutex>
class SyncCell {
public:
  class ScopedProxyLock {
  public:
    ScopedProxyLock(std::unique_lock<Lock_t> &&lock, T &instance)
      : lock_(std::move(lock)), instance_(instance) { }

    ~ScopedProxyLock() = default;

    // We would not want the RAII lock to somehow exit the scope where it's defined
    ScopedProxyLock(const ScopedProxyLock &)            = delete;
    ScopedProxyLock &operator=(const ScopedProxyLock &) = delete;
    ScopedProxyLock(ScopedProxyLock &&)                 = delete;
    ScopedProxyLock &operator=(ScopedProxyLock &&)      = delete;

    // Let the object act as a proxy for instance_
    T &operator*() { return instance_; }

    T *operator->() { return &instance_; }

  private:
    std::unique_lock<Lock_t> lock_;

    T &instance_;
  };

  SyncCell() = default;

  template<typename... Args>
  explicit SyncCell(Args &&...args) : instance_(std::forward<Args>(args)...) { }

  ~SyncCell() = default;

  SyncCell(const SyncCell &other) : instance_(*(other.get_locked())) { }

  SyncCell &operator=(const SyncCell &other) {
    if(&other != this) {
      // In this case we need to lock _both_ before we move from one to the other
      auto thisState  = this->get_locked();
      auto otherState = other.get_locked();

      *thisState = *otherState;
    }

    return *this;
  }

  // N.B. the mutex will be new, but we will move the rest of the state from the other instance
  SyncCell(SyncCell &&other) noexcept : instance_(std::move(*(other.get_locked()))) { }

  SyncCell &operator=(SyncCell &&other) noexcept {
    if(&other != this) {
      // In this case we need to lock _both_ before we move from one to the other
      auto thisState  = this->get_locked();
      auto otherState = other.get_locked();

      *thisState = std::move(*otherState);
    }

    return *this;
  }

  /**
   * Convenience method for std::condition_variable.
   *
   * Invokes the passed function as the condition variable predicate, and passes it a reference to
   * the shared state. This is safe becaues the condvar API has to lock the mutex before calling the
   * predicate.
   *
   * Parts of the std::condition_variable API require that a lock be passed in, and used with a
   * predicate function that assumes certain state is guarded by said lock. We opt for this
   * solution, since the less desirable alternatives are:
   *  - Expose lock_ via a getter, which would go against the abstraction which is the entire point
   *    of the class
   *  - Have this class own the condition_variable, but I want this class to be kept lean
   *
   * Passing in the condition_variable allows this class to in turn pass its internals to the
   * condition_variable's API, without exposing lock_ publicly.
   */
  template<typename Fn_t>
  // N.B. vanilla CV only works with std::mutex
  requires std::is_invocable_r_v<bool, Fn_t, const T &> && std::is_same_v<std::mutex, Lock_t>
  void cv_wait(std::condition_variable &condvar, Fn_t &&predicate) {
    std::unique_lock lck{lock_};
    condvar.wait(lck, [&, pred = std::forward<Fn_t>(predicate)] { return pred(instance_); });
  }

  template<typename Fn_t>
  // N.B. vanilla CV only works with std::mutex
  requires std::is_invocable_r_v<bool, Fn_t, const T &> && std::is_same_v<std::mutex, Lock_t>
  bool cv_wait_until(std::condition_variable &condvar, const TimePoint_t time, Fn_t &&predicate) {
    std::unique_lock lck{lock_};
    return condvar.wait_until(
      lck, std::move(time), [&, pred = std::forward<Fn_t>(predicate)] { return pred(instance_); });
  }

  [[nodiscard]] ScopedProxyLock get_locked() {
    return ScopedProxyLock(std::unique_lock<Lock_t>(lock_), instance_);
  }

  template<typename Callable_t>
  requires std::invocable<Callable_t, T &>
  std::invoke_result_t<Callable_t, T &> transact(const Callable_t &fn) {
    static_assert(!std::is_lvalue_reference_v<std::invoke_result_t<Callable_t, T &>>,
                  "SyncCell::transact shouldn't return a reference, as this could imply that "
                  "access to the protected atomic state is leaking out");
    std::scoped_lock lck(lock_);
    return fn(instance_);
  }

private:
  T              instance_;
  mutable Lock_t lock_;
};

}  // namespace mgfw
