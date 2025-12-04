#pragma once

#include "mgfw/SyncCell.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mgfw {

template<typename T>
concept is_cvar_t = std::same_as<T, int> || std::same_as<T, double> || std::same_as<T, bool>
                 || std::same_as<T, std::string>;

template<typename Raw_t, typename T = std::decay_t<Raw_t>>
// requires is_cvar_t<T>
std::conditional_t<std::same_as<T, std::string>, std::shared_ptr<T>, T> to_shared_if_needed(
  T &&val) {
  if constexpr(std::same_as<T, std::string>) {
    return std::make_shared<T>(std::forward<T &&>(val));
  }
  else {
    return T{std::forward<T &&>(val)};
  }
}

template<typename Raw_t, typename T = std::decay_t<Raw_t>>
requires is_cvar_t<T>
class CVar {
public:
  using CVarCallback_t = std::function<void(const T &prevVal, const T &newVal)>;

  // Template deduction gets funky if specify initialVal as a reference/rvlaue reference...
  CVar(std::string name, T initialVal, std::string desc = "")
    : val_{to_shared_if_needed<T>(std::move(initialVal))},
      name_(std::move(name)),
      desc_(std::move(desc)) { }

  const std::string &name() const { return name_; }

  const std::string &desc() const { return desc_; }

  T get() const {
    if constexpr(std::same_as<T, std::string>) {
      return *(val_.load(std::memory_order::acquire));
    }
    else {
      return val_.load(std::memory_order::acquire);
    }
  }

  void on_change(CVarCallback_t callback) {
    auto syncState = onChangeCallbacks_.get_locked();
    syncState->emplace_back(std::move(callback));
  }

  // Set value.
  // After setting the new value, invoke any callbacks, passing the previous value and the new
  // value.
  void set(const T &val) {
    auto syncState = onChangeCallbacks_.get_locked();

    T prev;
    if constexpr(std::same_as<T, std::string>) {
      prev = *(val_.load(std::memory_order::acquire));
      val_.store(std::make_shared<T>(val), std::memory_order::release);
    }
    else {
      prev = val_.load(std::memory_order::acquire);
      val_.store(val, std::memory_order::release);
    }

    // N.B. that we don't pass in references to val_ here in order to avoid races (e.g. set() is
    // called again from another thread while these callbacks are still being invoked)
    if(prev != val) {
      for(const auto &callback : *syncState) {
        callback(prev, val);
      }
    }
  }

private:
  std::atomic<std::conditional_t<std::same_as<T, std::string>, std::shared_ptr<T>, T>> val_;

  SyncCell<std::vector<CVarCallback_t>> onChangeCallbacks_;

  const std::string name_;
  const std::string desc_;
};

}  // namespace mgfw
