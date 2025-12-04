#pragma once

#include "mgfw/TypeHash.hpp"
#include "mgfw/TypeString.hpp"

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <utility>

namespace mgfw {

// Container providing type erasure
class TypeContainerBase {
public:
  explicit TypeContainerBase(const mgfw::Hash_t hsh) : hsh_(hsh) { }

  virtual ~TypeContainerBase()                            = default;
  TypeContainerBase(const TypeContainerBase &)            = default;
  TypeContainerBase &operator=(const TypeContainerBase &) = default;
  TypeContainerBase(TypeContainerBase &&)                 = default;
  TypeContainerBase &operator=(TypeContainerBase &&)      = default;

  /**
   * Return the TypeHash of the contained instance; used for a degree of type safety.
   */
  mgfw::Hash_t identity() const noexcept { return hsh_; }

protected:
  mgfw::Hash_t hsh_;
};

// Container for arbitrary types.
// We use this instead of std::any to support non-copyable types.
template<typename T>
class TypeContainer : public TypeContainerBase {
public:
  explicit TypeContainer(T &&instance)
  requires std::move_constructible<T>
    : TypeContainerBase(mgfw::TypeHash<T>), instance_(std::move(instance)) { }

  template<typename... Args>
  requires std::constructible_from<T, Args...>
  explicit TypeContainer(Args &&...args)
    : TypeContainerBase(mgfw::TypeHash<T>), instance_(std::forward<Args>(args)...) { }

  ~TypeContainer() override                       = default;
  TypeContainer(const TypeContainer &)            = default;
  TypeContainer &operator=(const TypeContainer &) = default;
  TypeContainer(TypeContainer &&)                 = default;
  TypeContainer &operator=(TypeContainer &&)      = default;

  T &get() noexcept { return instance_; }

private:
  T instance_;
};

/**
 * Maps a TypeHash to an instance of the corresponding type.
 */
class TypeMap {
public:
  using InstanceId_t = mgfw::U64;

  /**
   * We use -1 as the default, since clients will want to use `enum`/`enum class` as instance IDs,
   * and since the first value of the enum is 0 by default, we want to distinguish between the
   * default instance and the instance corresponding to that first value in the enum.
   */
  static constexpr InstanceId_t DEFAULT_INSTANCE_ID = std::numeric_limits<U64>::max();

  // Could also just use a tuple; this is a hair more descriptive
  struct MapKey {
    const mgfw::Hash_t hsh{};
    const InstanceId_t instanceId{};

    bool operator==(const MapKey &other) const {
      return this->hsh == other.hsh && this->instanceId == other.instanceId;
    }
  };

  // Stuff needed to use MapKey as the key in an unordered_map
  struct MapKeyEqual {
    bool operator()(const MapKey &lhs, const MapKey &rhs) const { return lhs == rhs; }
  };

  struct MapKeyHasher {
    std::size_t operator()(const MapKey &k) const {
      // TODO: this hash comes from a quick search; I have no idea if there is a better way to do
      // this (i.e. an easy way with less likelihood of collisions)
      return ((std::hash<mgfw::Hash_t>()(k.hsh) ^ (std::hash<InstanceId_t>()(k.instanceId) << 1U))
              >> 1U);
    }
  };

  bool contains(const mgfw::Hash_t hsh,
                const InstanceId_t instanceId = DEFAULT_INSTANCE_ID) const noexcept {
    return map_.contains({hsh, instanceId});
  }

  template<typename T>
  std::optional<std::reference_wrapper<T>> find(
    const InstanceId_t instanceId = DEFAULT_INSTANCE_ID) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    auto it = map_.find({hsh, instanceId});

    if(it == map_.end()) {
      return std::nullopt;
    }

    return extract_from_ctr_base<T>(*(it->second.get()));
  }

  /**
   * Creates an instance of type T and places it in the map. args are passed
   * to T's constructor.
   */
  template<typename T, InstanceId_t instanceId = DEFAULT_INSTANCE_ID, typename... Args>
  T &emplace(Args &&...args) {
    constexpr auto hsh = TypeHash<T>;

    /**
     * N.B. we have to explicitly create the TypeContainer here; we can't just forward
     * the args to try_emplace by themselves, as the map would not know which type of
     * container to create.
     */
    auto [iter, success] = map_.emplace(
      MapKey{hsh, instanceId}, std::make_unique<TypeContainer<T>>(std::forward<Args>(args)...));

    if(!success) {
      throw std::runtime_error(std::format(
        "Failed to emplace instance of {}; was it called more than once?", mgfw::TypeString<T>));
    }

    return extract_from_ctr_base<T>(*(iter->second.get()));
  }

  template<typename T>
  T &insert(T &&val, const InstanceId_t instanceId = DEFAULT_INSTANCE_ID) {
    constexpr auto hsh = TypeHash<T>;

    auto [iter, success] = map_.emplace(MapKey{hsh, instanceId},
                                        std::make_unique<TypeContainer<T>>(std::forward<T>(val)));

    if(!success) {
      throw std::runtime_error(std::format(
        "Failed to insert instance of {}; was it called more than once?", mgfw::TypeString<T>));
    }

    return extract_from_ctr_base<T>(*(iter->second.get()));
  }

  void erase(const mgfw::Hash_t hsh, const InstanceId_t instanceId) {
    map_.erase({hsh, instanceId});
  }

  /**
   * Retrieve a reference to the type T in the map, throwing if it does not exist
   */
  template<typename T, InstanceId_t instanceId = DEFAULT_INSTANCE_ID>
  T &get_ref() {
    constexpr auto   hsh = mgfw::TypeHash<T>;
    constexpr MapKey mapKey{hsh, instanceId};
    assert(map_.contains(mapKey) || "get_ref<T>() called without T in map_");

    return extract_from_ctr_base<T>(*(map_.at(mapKey).get()));
  }

private:

  // Extract a reference to the contained type instance
  template<typename T>
  T &extract_from_ctr_base(TypeContainerBase &pContainerBase) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    if(!(pContainerBase.identity() == hsh)) {
      throw std::runtime_error("extract_from_ctr_base<T> has an entry with a typehash != T");
    }

    return reinterpret_cast<TypeContainer<T> &>(pContainerBase).get();
  }

  std::unordered_map<MapKey, std::unique_ptr<TypeContainerBase>, MapKeyHasher, MapKeyEqual> map_;
};

}  // namespace mgfw
