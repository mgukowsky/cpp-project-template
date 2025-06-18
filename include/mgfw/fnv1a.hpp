#pragma once

#include "mgfw/types.hpp"

#include <string_view>

namespace mgfw {

namespace detail_ {

  constinit const U32 FNV_BASIS_32 = 0x811C'9DC5;
  constinit const U32 FNV_PRIME_32 = 0x0100'0193;
  constinit const U64 FNV_BASIS_64 = 0xCBF2'9CE4'8422'2325;
  constinit const U64 FNV_PRIME_64 = 0x0000'0100'0000'01B3;

  template<typename HashSize_t, HashSize_t fnv_basis, HashSize_t fnv_prime>
  consteval HashSize_t fnv1a_hash(std::string_view sv) noexcept {
    HashSize_t hash = fnv_basis;

    for(const auto c : sv) {
      hash = (hash ^ static_cast<HashSize_t>(c)) * fnv_prime;
    }
    return hash;
  }

}  // namespace detail_

consteval U32 fnv1a_hash_32(const std::string_view sv) noexcept {
  return detail_::fnv1a_hash<U32, detail_::FNV_BASIS_32, detail_::FNV_PRIME_32>(sv);
}

consteval U64 fnv1a_hash_64(const std::string_view sv) noexcept {
  return detail_::fnv1a_hash<U64, detail_::FNV_BASIS_64, detail_::FNV_PRIME_64>(sv);
}

using Hash_t = U32;

}  // namespace mgfw
