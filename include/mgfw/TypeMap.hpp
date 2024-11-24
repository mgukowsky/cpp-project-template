#pragma once

#include "mgfw/TypeHash.hpp"

#include <cassert>
#include <concepts>
#include <map>
#include <memory>
#include <utility>

namespace mgfw {

namespace detail_ {

  // Container providing type erasure
  class TypeContainerBase {
  public:
    explicit TypeContainerBase(const mgfw::Hash_t hsh) : hsh_(hsh) { }

    virtual ~TypeContainerBase() = default;

    /**
     * Return the TypeHash of the contained instance; used for a degree of type safety.
     */
    inline mgfw::Hash_t identity() const noexcept { return hsh_; }

  protected:
    mgfw::Hash_t hsh_;
  };

  // Container for arbitrary types.
  // We use this instead of std::any to support non-copyable types.
  template<typename T>
  class TypeContainer : public TypeContainerBase {
  public:
    template<typename... Args>
    requires std::constructible_from<T, Args...>
    TypeContainer(Args &&...args)
      : TypeContainerBase(mgfw::TypeHash<T>), instance_(std::forward<Args>(args)...) { }

    T &get() noexcept { return instance_; }

  private:
    T instance_;
  };

}  // namespace detail_

/**
 * Maps a TypeHash to an instance of the corresponding type.
 */
class TypeMap {
public:
  bool contains(const mgfw::Hash_t hsh) const noexcept { return map_.contains(hsh); }

  /**
   * Creates an instance of type T and places it in the map. args are passed
   * to T's constructor. If type T does not yet exist in the map, an instance of
   * T is created, and this function returns false. Otherwise, this function does
   * nothing and returns true.
   */
  template<typename T, typename... Args>
  bool emplace(Args &&...args) {
    /**
     * N.B. we have to explicitly create the TypeContainer here; we can't just forward
     * the args to try_emplace by themselves, as the map would not know which type of
     * container to create.
     */

    if(contains(TypeHash<T>)) {
      return true;
    }

    map_.emplace(mgfw::TypeHash<T>,
                 std::make_unique<detail_::TypeContainer<T>>(std::forward<Args>(args)...));
    return false;
  }

  void erase(const mgfw::Hash_t hsh) { map_.erase(hsh); }

  /**
   * Retrieve a reference to the type T in the map, throwing if it does not exist
   */
  template<typename T>
  T &get_ref() {
    constexpr auto hsh = mgfw::TypeHash<T>;
    assert(map_.contains(hsh) || "get_ref<T>() called without T in map_");

    // Let this throw if there is no entry for T
    auto pContainerBase = map_.at(hsh).get();
    assert(pContainerBase->identity() == hsh || "get_ref<T> has an entry with a typehash != T");

    return reinterpret_cast<detail_::TypeContainer<T> *>(pContainerBase)->get();
  }

private:
  std::map<mgfw::Hash_t, std::unique_ptr<detail_::TypeContainerBase>> map_;
};

}  // namespace mgfw
