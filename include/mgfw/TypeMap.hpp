#pragma once

#include "mgfw/TypeHash.hpp"
#include "mgfw/TypeString.hpp"

#include <cassert>
#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <optional>
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
  bool contains(const mgfw::Hash_t hsh) const noexcept { return map_.contains(hsh); }

  template<typename T>
  std::optional<std::reference_wrapper<T>> find() {
    constexpr auto hsh = mgfw::TypeHash<T>;

    auto it = map_.find(hsh);

    if(it == map_.end()) {
      return std::optional<std::reference_wrapper<T>>();
    }

    return extract_from_ctr_base<T>(*(it->second.get()));
  }

  /**
   * Creates an instance of type T and places it in the map. args are passed
   * to T's constructor.
   */
  template<typename T, typename... Args>
  T &emplace(Args &&...args) {
    constexpr auto hsh = TypeHash<T>;

    /**
     * N.B. we have to explicitly create the TypeContainer here; we can't just forward
     * the args to try_emplace by themselves, as the map would not know which type of
     * container to create.
     */
    auto [iter, success] =
      map_.emplace(hsh, std::make_unique<TypeContainer<T>>(std::forward<Args>(args)...));

    if(!success) {
      throw std::runtime_error(std::format(
        "Failed to emplace instance of {}; was it called more than once?", mgfw::TypeString<T>));
    }

    return extract_from_ctr_base<T>(*(iter->second.get()));
  }

  template<typename T>
  T &insert(T &&val) {
    constexpr auto hsh = TypeHash<T>;

    auto [iter, success] =
      map_.emplace(hsh, std::make_unique<TypeContainer<T>>(std::forward<T>(val)));

    if(!success) {
      throw std::runtime_error(std::format(
        "Failed to insert instance of {}; was it called more than once?", mgfw::TypeString<T>));
    }

    return extract_from_ctr_base<T>(*(iter->second.get()));
  }

  void erase(const mgfw::Hash_t hsh) { map_.erase(hsh); }

  /**
   * Retrieve a reference to the type T in the map, throwing if it does not exist
   */
  template<typename T>
  T &get_ref() {
    constexpr auto hsh = mgfw::TypeHash<T>;
    assert(map_.contains(hsh) || "get_ref<T>() called without T in map_");

    return extract_from_ctr_base<T>(*(map_.at(hsh).get()));
  }

private:

  // Extract a reference to the contained type instance
  template<typename T>
  T &extract_from_ctr_base(TypeContainerBase &pContainerBase) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    // assert(pContainerBase || "extract_from_ctr_base<T>: pContainerBase is null");
    assert(pContainerBase.identity() == hsh
           || "extract_from_ctr_base<T> has an entry with a typehash != T");

    return reinterpret_cast<TypeContainer<T> &>(pContainerBase).get();
  }

  std::map<mgfw::Hash_t, std::unique_ptr<TypeContainerBase>> map_;
};

}  // namespace mgfw
