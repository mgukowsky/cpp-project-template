#pragma once

#include <mutex>
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

  ~SyncCell()                           = default;
  SyncCell(const SyncCell &)            = delete;
  SyncCell &operator=(const SyncCell &) = delete;
  SyncCell(SyncCell &&)                 = default;
  SyncCell &operator=(SyncCell &&)      = default;

  [[nodiscard]] ScopedProxyLock get_lock() {
    return ScopedProxyLock(std::unique_lock<Lock_t>(lock_), instance_);
  }

private:
  T      instance_;
  Lock_t lock_;
};

}  // namespace mgfw
