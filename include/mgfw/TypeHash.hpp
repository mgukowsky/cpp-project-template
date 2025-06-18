#pragma once

#include "mgfw/fnv1a.hpp"

#include <source_location>
#include <string_view>
#include <type_traits>

/**
 * Utility to create a unique hash for individual types at compile time.
 *
 * Based on https://github.com/Manu343726/ctti/blob/master/include/ctti/detail_/hash.hpp
 */
namespace mgfw {

namespace detail_ {
  /**
   * Helper function to generate a unique string depending on the type argument. This works because
   * std::source_location::current() returns a string containing the template parameter's typename.
   */
  template<typename T>
  consteval std::string_view uid_helper() noexcept {
    return std::source_location::current().function_name();
  }
} /* namespace detail_ */

// N.B. that both the 32 and 64 bit versions will decay the type
template<typename T>
constinit const inline auto TypeHash32 = fnv1a_hash_32(detail_::uid_helper<std::decay_t<T>>());

template<typename T>
constinit const inline auto TypeHash64 = fnv1a_hash_64(detail_::uid_helper<std::decay_t<T>>());

// Use the 32 bit type hash by default
template<typename T>
constinit const inline auto TypeHash = TypeHash32<T>;

}  // namespace mgfw
