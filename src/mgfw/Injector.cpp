#include "mgfw/Injector.hpp"

#include "mgfw/fnv1a.hpp"

#include <algorithm>
#include <ranges>

namespace mgfw {

Injector::~Injector() {
  auto state = stateCell_.get_locked();
  std::ranges::for_each(std::ranges::reverse_view(state->instantiationList_),
                        [&](const mgfw::Hash_t hsh) { state->typeMap_.erase(hsh); });
}

}  // namespace mgfw
