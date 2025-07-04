#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace mgfw {

template<typename T>
concept is_cvar_t = std::same_as<T, int> || std::same_as<T, double> || std::same_as<T, bool>
                 || std::same_as<T, std::string>;

template<typename T>
requires is_cvar_t<T>
class CVar {
public:
  using CVarCallback_t = std::function<void(const T &prevVal, const T &newVal)>;

  CVar(std::string name, const T initialVal, std::string desc = "")
    : val_(std::move(initialVal)), name_(std::move(name)), desc_(std::move(desc)) { }

  const std::string &name() const { return name_; }

  const std::string &desc() const { return desc_; }

  const T &get() const { return val_; }

  void on_change(CVarCallback_t callback) { onChangeCallbacks_.emplace_back(std::move(callback)); }

  // Set value.
  // After setting the new value, invoke any callbacks, passing the previous value and the new
  // value.
  void set(const T &val) {
    const T prev = val_;

    val_ = val;

    if(prev != val_) {
      for(const auto &callback : onChangeCallbacks_) {
        callback(prev, val_);
      }
    }
  }

private:
  T val_;

  std::vector<CVarCallback_t> onChangeCallbacks_;

  const std::string name_;
  const std::string desc_;
};

}  // namespace mgfw
