#pragma once

#include "mgfw/TypeString.hpp"

#include <string>
#include <utility>
#include <variant>

namespace mgfw {

template<typename T>
concept is_cvar_t = std::same_as<T, int> || std::same_as<T, double> || std::same_as<T, bool>
                 || std::same_as<T, std::string>;

class CVar {
public:
  using Value_t = std::variant<int, double, bool, std::string>;

  CVar(std::string name, Value_t initialVal, std::string desc = "")
    : val_(std::move(initialVal)), name_(std::move(name)), desc_(std::move(desc)) { }

  const std::string &name() const { return name_; }

  const std::string &desc() const { return desc_; }

  // Get value as T; throws if wrong type.
  // Eventually may write something to coerce to the requested type
  template<typename T>
  requires is_cvar_t<T>
  const T &get() const {
    assert_has_type_<T>();
    return std::get<T>(val_);
  }

  // Set value as T; throws if wrong type.
  template<typename T>
  requires is_cvar_t<T>
  void set(const T &val) {
    assert_has_type_<T>();
    val_ = val;
  }

private:
  template<typename T>
  requires is_cvar_t<T>
  void assert_has_type_() const {
    if(!std::holds_alternative<T>(val_)) {
      throw std::runtime_error(
        std::format("Expected {}, but type index was {} ({})", TypeString<T>, val_.index(), name_));
    }
  }

  Value_t val_;

  const std::string name_;
  const std::string desc_;
};

}  // namespace mgfw
