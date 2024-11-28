#include "mgfw/Injector.hpp"

#include "mgfw/TypeHash.hpp"

#include <algorithm>
#include <ranges>

namespace mgfw {

Injector::Injector() {
  // TODO
}

Injector::~Injector() {
  std::ranges::for_each(std::ranges::reverse_view(instantiationList_),
                        [&](const mgfw::Hash_t hsh) { typeMap_.erase(hsh); });
}

}  // namespace mgfw
