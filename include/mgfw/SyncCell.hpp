#pragma once

#include <mutex>
#include <type_traits>
#include <utility>

namespace mgfw {

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

  [[nodiscard]] ScopedProxyLock get_locked() {
    return ScopedProxyLock(std::unique_lock<Lock_t>(lock_), instance_);
  }

private:
  T              instance_;
  mutable Lock_t lock_;
};

}  // namespace mgfw
