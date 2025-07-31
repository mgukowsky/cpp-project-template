#pragma once

#include "mgfw/SyncCell.hpp"
#include "mgfw/TypeHash.hpp"
#include "mgfw/TypeMap.hpp"
#include "mgfw/TypeString.hpp"
#include "mgfw/defer.hpp"
#include "mgfw/types.hpp"

#include <any>
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace mgfw {

class Injector {
public:
  /**
   * Ensure we strip off all pointers/qualifiers/etc. from any types used with the Injector
   * interface. N.B. this will not fully decay pointers to pointers.
   */
  template<typename T>
  using InjType_t = std::remove_pointer_t<std::decay_t<T>>;

  /**
   * 'Recipes' are functions that return a new instance of type T.
   */
  template<typename T>
  using Recipe_t = std::function<T(Injector &)>;

  /**
   * Recipes for interface (abstract) types can _only_ return a reference
   */
  template<typename T>
  using IfaceRecipe_t = std::function<T &(Injector &)>;

  Injector() = default;

  /**
   * Deletes all entries in the typeMap_ in the opposite order in which they were constructed.
   */
  ~Injector();

  /**
   * Moving is OK, but no copying (mainly because I don't want to write the deep copy logic XD )
   */
  Injector(const Injector &)            = delete;
  Injector &operator=(const Injector &) = delete;
  Injector(Injector &&)                 = default;
  Injector &operator=(Injector &&)      = default;

  template<typename Raw_t, typename... Args>
  requires(!std::is_abstract_v<Raw_t>)
  void add_ctor_recipe() {
    // Can't do this in the template declaration, so we have to do it here
    using T = InjType_t<Raw_t>;
    static_assert(std::constructible_from<T, Args...>,
                  "Injector::add_ctor_recipe<T, ...Ts> will only accept Ts if T has a constructor "
                  "that accepts the arguments (Ts...)");

    auto recipe = [](Injector &injector) {
      return T(injector.ctor_arg_dispatcher_<Args>(injector)...);
    };
    add_recipe<T>(recipe);
  }

  // We're using a universal reference but intentionally not using std::forward, since we want to
  // copy the recipe into the lambda that will be used later
  // NOLINTBEGIN
  template<typename Raw_t, typename T = InjType_t<Raw_t>, typename RecipeFn_t>
  requires std::is_invocable_r_v<T, RecipeFn_t, Injector &>
  void add_recipe(RecipeFn_t &&recipe) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    auto state = stateCell_.get_locked();

    if(state->recipeMap_.contains(hsh)) {
      throw std::runtime_error(
        std::format("Injector::add_recipe invoked for type {}, but a recipe was already added",
                    mgfw::TypeString<T>));
    }

    state->recipeMap_.emplace(
      hsh,
      std::make_pair(RecipeType_t::CONCRETE,
                     std::make_any<Recipe_t<T>>([recipe](Injector &inj) { return recipe(inj); })));
  }

  // NOLINTEND

  template<typename RawImpl_t,
           typename RawIface_t,
           typename Impl_t  = InjType_t<RawImpl_t>,
           typename Iface_t = InjType_t<RawIface_t>>
  requires std::derived_from<Impl_t, Iface_t>
  void bind_impl() {
    constexpr auto hsh = mgfw::TypeHash<Iface_t>;

    auto state = stateCell_.get_locked();

    if(state->recipeMap_.contains(hsh)) {
      throw std::runtime_error(
        std::format("Injector::bind_impl invoked for type {}, but a recipe was already added",
                    mgfw::TypeString<Iface_t>));
    }

    state->recipeMap_.emplace(
      hsh,
      std::make_pair(RecipeType_t::INTERFACE,
                     std::make_any<IfaceRecipe_t<Iface_t>>(
                       [](Injector &injector) -> Iface_t & { return injector.get<Impl_t>(); })));
  }

  template<typename Raw_t, typename T = InjType_t<Raw_t>>
  // We don't need the type here to be move constructible, because we can rely on RVO to only create
  // the instance once and 'move' it out of the function
  requires(!std::is_abstract_v<T>)
  T create() {
    return make_dependency_<T>(DepType_t::NEW_VALUE);
  }

  template<typename Raw_t, typename T = InjType_t<Raw_t>>
  requires std::is_abstract_v<T> || std::move_constructible<T>
  T &get() {
    constexpr auto hsh = mgfw::TypeHash<T>;

    auto state = stateCell_.get_locked();

    // Asking for an instance of this class is an identity function.
    if constexpr(hsh == TypeHash<Injector>) {
      return *this;
    }
    // We need to put this behind an if constexpr() path to prevent the compiler from generating
    // code for make_dependency_<AbstractClass>() and typeMap_.get_ref<AbstractClass>(), neither
    // of which would compile
    else if constexpr(std::is_abstract_v<T>) {
      auto iter = state->recipeMap_.find(hsh);
      if(iter != state->recipeMap_.end()) {
        auto &[recipeType, recipeFn] = iter->second;
        if(recipeType != RecipeType_t::INTERFACE) {
          throw std::runtime_error(
            std::format("Found recipe for type {}, but could use it because T is an abstract type "
                        "and the recipe does not return a reference",
                        mgfw::TypeString<T>));
        }
        return std::any_cast<IfaceRecipe_t<T>>(recipeFn)(*this);
      }
      else {
        throw std::runtime_error(
          std::format("Could not create instance of type {} because it is abstract and there is "
                      "no recipe for it. Perhaps use Injector::bind_impl<Impl, {}>()",
                      mgfw::TypeString<T>,
                      mgfw::TypeString<T>));
      }
    }
    else {
      auto optionalRef = state->typeMap_.find<T>();
      if(!optionalRef.has_value()) {
        // It's possible that we want to return a non-abstract interface type that was set up with
        // bind-impl. If this is the case then there won't be an entry in the type map, but we still
        // want to invoke the recipe and return the reference to the interface.
        auto iter = state->recipeMap_.find(hsh);

        // Awkward naming, but iter->second is the pair of [recipeType, recipeFn]
        if(iter != state->recipeMap_.end() && iter->second.first == RecipeType_t::INTERFACE) {
          return std::any_cast<IfaceRecipe_t<T>>(iter->second.second)(*this);
        }
        else {
          return state->typeMap_.insert(make_dependency_<T>(DepType_t::REFERENCE));
        }
      }

      return optionalRef.value();
    }
  }

private:
  /**
   * A tag for the type of dependency being requested.
   */
  enum class DepType_t : U8 {
    /**
     * Return a reference to an existing instance of a dependency (or create one if one doesn't
     * exist).
     */
    REFERENCE,

    /**
     * Create a fresh instance of the requested dependency.
     */
    NEW_VALUE
  };

  /**
   * Tag for the type of a recipe.
   */
  enum class RecipeType_t : U8 {
    /**
     * 'Normal' type; returns a value.
     */
    CONCRETE,

    /**
     * Interface types; returns a reference.
     * Necessary for abstract types, since they cannot be instantiated.
     */
    INTERFACE,
  };

  /**
   * Used internally by add_ctor_recipe, dispatches to the appropriate behavior depending on the
   * qualifiers of Raw_t. N.B. that a new instance of T will be created if Raw_t is neither a
   * reference nor a pointer.
   */
  template<
    typename Raw_t,
    typename T = std::conditional_t<std::is_lvalue_reference_v<Raw_t>, Raw_t, InjType_t<Raw_t>>>
  std::conditional_t<std::is_pointer_v<Raw_t>, Raw_t, T> ctor_arg_dispatcher_(Injector &injector) {
    if constexpr(std::is_lvalue_reference_v<T>) {
      return injector.get<T>();
    }
    else if constexpr(std::is_pointer_v<Raw_t>) {
      return &(injector.get<T>());
    }
    else {
      return injector.create<T>();
    }
  }

  /**
   * Create an instance of the requested dependency.
   *
   * If a recipe for a type exists, then the recipe will be invoked. Otherwise, we will attempt to
   * default-construct an instance of T. If that fails, then we throw an exception.
   */
  template<typename T>
  T make_dependency_(const DepType_t depType) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    auto state = stateCell_.get_locked();

    if(state->typeHashStack_.contains(hsh)) {
      throw std::runtime_error(
        std::format("Dependency cycle detected for type {}", mgfw::TypeString<T>));
    }

    state->typeHashStack_.insert(hsh);

    // Use `defer` to run this code to pop off the stack after we've returned
    const mgfw::defer deferred([&] { state->typeHashStack_.erase(hsh); });

    // If we're not calling with create(), then we're creating the instance being placed in the
    // type map, so record when it was instantiated.
    if(depType != DepType_t::NEW_VALUE) {
      state->instantiationList_.push_back(hsh);
    }

    auto iter = state->recipeMap_.find(hsh);
    if(iter != state->recipeMap_.end()) {
      auto &[recipeType, recipeFn] = iter->second;
      if(recipeType != RecipeType_t::CONCRETE) {
        throw std::runtime_error(std::format(
          "make_dependency_ called for type {}, but an invalid recipe was found of type {}",
          mgfw::TypeString<T>,
          std::to_underlying(recipeType)));
      }

      return std::any_cast<Recipe_t<T>>(recipeFn)(*this);
    }

    if constexpr(std::default_initializable<T>) {
      return T();
    }
    else {
      throw std::runtime_error(
        std::format("Could not create instance of type {} because it was not default initializable "
                    "and there was no recipe available. Perhaps use Injector::add_ctor_recipe()",
                    mgfw::TypeString<T>));
    }
  }

  struct State_ {
    /**
     * Tracks the order in which instances are created in the typeMap_; used to ensure that
     * dependencies are destroyed in the correct order, i.e. we destroy the dependencies in the
     * reverse order in which they are created to ensure we don't destroy a dependency before its
     * dependent(s).
     */
    std::vector<mgfw::Hash_t> instantiationList_;

    /**
     * Functions used to create new instances of types
     */
    std::map<mgfw::Hash_t, std::pair<RecipeType_t, std::any>> recipeMap_;

    /**
     * Tracks the types that are currently being injected; used to detect cycles. Though we use this
     * as a stack, we choose a set since we'll be searching it frequently and won't have duplicate
     * entries.
     */
    std::set<mgfw::Hash_t> typeHashStack_;

    /**
     * Contains cached instances of given types
     */
    mgfw::TypeMap typeMap_;
  };

  // N.B. that we need the recursive mutex given the recursive nature of dependency injection.
  SyncCell<State_, std::recursive_mutex> stateCell_;
};

}  // namespace mgfw
