#include "mgfw/Injector.hpp"

#include "mgfw/fnv1a.hpp"

#include <algorithm>
#include <ranges>

namespace mgfw {

Injector::~Injector() {
  auto state = stateCell_.get_locked();
  std::ranges::for_each(std::ranges::reverse_view(state->instantiationList_),
                        [&](const MapKey k) { state->typeMap_.erase(k.hsh, k.instanceId); });
}

}  // namespace mgfw
