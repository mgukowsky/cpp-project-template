#pragma once

#include "mgfw/fnv1a.hpp"

#include <string_view>

namespace mgfw {

consteval Hash_t string_key(std::string_view sv) { return fnv1a_hash_32(sv); }

}  // namespace mgfw
