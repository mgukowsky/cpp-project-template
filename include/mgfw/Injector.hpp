#pragma once

#include "mgfw/TypeHash.hpp"
#include "mgfw/TypeMap.hpp"
#include "mgfw/TypeString.hpp"
#include "mgfw/defer.hpp"

#include <any>
#include <format>
#include <functional>
#include <map>
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

  template<typename T>
  using Recipe_t = std::function<T(Injector &)>;

  template<typename T>
  using IfaceRecipe_t = std::function<T &(Injector &)>;

  /**
   * The first thing that the Injector does upon construction is add itself to its typeMap_ as the
   * entry for the Injector type. This allows for recipes that have a dependency on the Injector
   * itself to return an instance of this class as that dependency. Also adds a specialized recipe
   * to return a "child" Injector (i.e. a new Injector with pUpstream_ set to the called Injector
   * instance) when creat() is invoked.
   */
  Injector();

  /**
   * Deletes all entries in the typeMap_ in the opposite order in which they were constructed.
   */
  ~Injector();

  /**
   * No copying, no moving
   */
  Injector(const Injector &)            = delete;
  Injector &operator=(const Injector &) = delete;
  Injector(Injector &&)                 = delete;
  Injector &operator=(Injector &&)      = delete;

  template<typename Raw_t, typename... Args>
  void add_ctor_recipe() {
    // Can't do this in the template declaration, so we have to do it here
    using T = InjType_t<Raw_t>;
    static_assert(std::constructible_from<T, Args...>,
                  "Injector#addCtorRecipe<T, ...Ts> will only accept Ts if T has a constructor "
                  "that accepts the arguments (Ts...)");

    auto recipe = [](Injector &injector) {
      return T(injector.ctor_arg_dispatcher_<Args>(injector)...);
    };
    add_recipe<T>(recipe);
  }

  template<typename Raw_t, typename T = InjType_t<Raw_t>, typename RecipeFn_t>
  requires std::is_invocable_r_v<T, RecipeFn_t, Injector &>
  void add_recipe(RecipeFn_t &&recipe) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    if(recipeMap_.contains(hsh)) {
      // TODO: log that overriding
    }

    recipeMap_.emplace(hsh,
                       std::make_any<Recipe_t<T>>([recipe](Injector &inj) { return recipe(inj); }));
  }

  template<typename RawImpl_t,
           typename RawIface_t,
           typename Impl_t  = InjType_t<RawImpl_t>,
           typename Iface_t = InjType_t<RawIface_t>>
  requires std::derived_from<Impl_t, Iface_t>
  void bind_impl() {
    constexpr auto hsh = mgfw::TypeHash<Iface_t>;

    if(ifaceRecipeMap_.contains(hsh)) {
      // TODO: log that overriding
    }

    ifaceRecipeMap_.emplace(
      hsh, std::make_any<IfaceRecipe_t>([](Injector &injector) { return injector.get<Impl_t>(); }));
  }

  template<typename Raw_t, typename T = InjType_t<Raw_t>>
  requires(!std::is_abstract_v<T>)
  T create() {
    return make_dependency_<T>(DepType_t::NEW_VALUE);
  }

  template<typename Raw_t, typename T = InjType_t<Raw_t>>
  T &get() {
    constexpr auto hsh = mgfw::TypeHash<T>;
    if(!typeMap_.contains(hsh)) {
      if(ifaceRecipeMap_.contains(hsh)) {
        return std::any_cast<IfaceRecipe_t<T>>(recipeMap_.at(hsh))(*this);
      }
      else {
        typeMap_.insert(make_dependency_<T>(DepType_t::REFERENCE));
      }
    }

    // TODO: we are performing the lookup for <T> in the typemap twice
    return typeMap_.get_ref<T>();
  }

private:
  /**
   * A tag for the type of dependency being requested.
   */
  enum class DepType_t {
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
   * TODO: add notes for impl/abstract check
   */
  template<typename T>
  T make_dependency_(const DepType_t depType) {
    constexpr auto hsh = mgfw::TypeHash<T>;

    if(typeHashStack_.contains(hsh)) {
      throw std::runtime_error(
        std::format("Dependency cycle detected for type {}", mgfw::TypeString<T>));
    }

    typeHashStack_.insert(hsh);
    mgfw::defer deferred([&] { typeHashStack_.erase(hsh); });

    // If we're not calling with create(), then we're creating the instance being placed in the type
    // map, so record when it was instantiated.
    if(depType != DepType_t::NEW_VALUE) {
      instantiationList_.push_back(hsh);
    }

    if(recipeMap_.contains(hsh)) {
      return std::any_cast<Recipe_t<T>>(recipeMap_.at(hsh))(*this);
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

  /**
   * Tracks the order in which instances are created in the typeMap_; used to ensure that
   * dependencies are destroyed in the correct order, i.e. we destroy the dependencies in the
   * reverse order in which they are created to ensure we don't destroy a dependency before its
   * dependent(s).
   */
  std::vector<mgfw::Hash_t> instantiationList_;

  std::map<mgfw::Hash_t, std::any> recipeMap_;
  std::map<mgfw::Hash_t, std::any> ifaceRecipeMap_;

  /**
   * Tracks the types that are currently being injected; used to detect cycles. Though we use this
   * as a stack, we choose a set since we'll be searching it frequently and won't have duplicate
   * entries.
   */
  std::set<mgfw::Hash_t> typeHashStack_;

  mgfw::TypeMap typeMap_;
};

}  // namespace mgfw
