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

  SyncCell(const SyncCell &other)
    // Use IIFE to ensure we lock the other instance before copying it. If we didn't do this, then
    // we would have to default construct this->instance_ in the initializer list (and have to
    // require that SyncCell's T be default constructible) and then copy construct it in the body.
    : instance_([&] {
        std::unique_lock<Lock_t> otherLock(other.lock_);
        return other.instance_;
      }()) { }

  SyncCell &operator=(const SyncCell &other) {
    if(&other != this) {
      std::unique_lock<Lock_t> thisLock(this->lock_);
      std::unique_lock<Lock_t> otherLock(other.lock_);
      this->instance_ = other.instance_;
    }
  }

  // NOLINTBEGIN
  SyncCell(SyncCell &&other) noexcept
    : instance_([&](SyncCell &&otherInner) {
        std::unique_lock<Lock_t> otherLock(otherInner.lock_);
        return otherInner.instance_;
      }(std::move(other))) { }

  // NOLINTEND

  SyncCell &operator=(SyncCell &&other) noexcept {
    if(&other != this) {
      std::unique_lock<Lock_t> thisLock(this->lock_);
      std::unique_lock<Lock_t> otherLock(other.lock_);
      this->instance_ = std::move(other.instance_);
    }
  }

  /**
   * Convenience method for std::condition_variable.
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
  requires std::is_invocable_r_v<bool, Fn_t> && std::is_same_v<std::mutex, Lock_t>
  void cv_wait(std::condition_variable &condvar, Fn_t &&predicate) {
    std::unique_lock lck{lock_};
    condvar.wait(lck, std::forward(predicate));
  }

  template<typename Fn_t>
  // N.B. vanilla CV only works with std::mutex
  requires std::is_invocable_r_v<bool, Fn_t> && std::is_same_v<std::mutex, Lock_t>
  bool cv_wait_until(std::condition_variable &condvar, Fn_t &&predicate, const TimePoint_t &&time) {
    std::unique_lock lck{lock_};
    return condvar.wait_until(lck, std::move(time), std::forward(predicate));
  }

  [[nodiscard]] ScopedProxyLock get_locked() {
    return ScopedProxyLock(std::unique_lock<Lock_t>(lock_), instance_);
  }

private:
  T              instance_;
  mutable Lock_t lock_;
};

}  // namespace mgfw
