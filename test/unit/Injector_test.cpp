#include "mgfw/Injector.hpp"

#include <gtest/gtest.h>

#include <string_view>

using mgfw::Injector;

namespace {

#define MAKE_DEP_CLASS(id)                 \
  struct Dep##id {                         \
    Dep##id() { ++instanceCounter; }       \
    inline static int instanceCounter = 0; \
  };

MAKE_DEP_CLASS(A);
MAKE_DEP_CLASS(B);
MAKE_DEP_CLASS(C);

#undef MAKE_DEP_CLASS

struct LifetimeTracker {
  LifetimeTracker() { ++numCtorCalls; }

  ~LifetimeTracker() { ++numDtorCalls; }

  LifetimeTracker(const LifetimeTracker &) { ++numCopyCalls; }

  LifetimeTracker &operator=(const LifetimeTracker &rhs) {
    if(this != &rhs) {
      // pass
    }
    ++numCopyCalls;
    return *this;
  }

  LifetimeTracker(LifetimeTracker &&) noexcept { ++numMoveCalls; }

  LifetimeTracker &operator=(LifetimeTracker &&) noexcept {
    ++numMoveCalls;
    return *this;
  }

  inline static int numCtorCalls = 0;
  inline static int numCopyCalls = 0;
  inline static int numDtorCalls = 0;
  inline static int numMoveCalls = 0;
};

class Injector_test : public ::testing::Test {
protected:
  void SetUp() override {
    DepA::instanceCounter         = 0;
    DepB::instanceCounter         = 0;
    DepC::instanceCounter         = 0;
    LifetimeTracker::numCtorCalls = 0;
    LifetimeTracker::numCopyCalls = 0;
    LifetimeTracker::numDtorCalls = 0;
    LifetimeTracker::numMoveCalls = 0;
    dtorTracker.clear();
  }

  inline static std::vector<std::string> dtorTracker = {};
};

}  // namespace

TEST_F(Injector_test, getAndCreate) {
  class DefaultCtorClass { };

  Injector    inj;
  const auto &ref1 = inj.get<DefaultCtorClass>();
  const auto &ref2 = inj.get<DefaultCtorClass>();

  EXPECT_EQ(&ref1, &ref2) << "Injector::get<T>() should always return the same instance";

  // N.B. that we are creating a ref, but we are binding it to a temporary and therefore extending
  // its lifetime.
  const auto &ref3 = inj.create<DefaultCtorClass>();
  EXPECT_NE(&ref1, &ref3) << "Injector::create<T>() should return a new instance";
}

TEST_F(Injector_test, lifetimeTracker) {
  Injector inj;

  [[maybe_unused]] const auto &l = inj.get<LifetimeTracker>();
  EXPECT_EQ(1, LifetimeTracker::numCtorCalls);
  EXPECT_EQ(0, LifetimeTracker::numCopyCalls);
  EXPECT_EQ(1, LifetimeTracker::numMoveCalls);  // one move is needed to place the entry into the
                                                // type container

  // No construction should be needed once the instance is cached
  [[maybe_unused]] const auto &l2 = inj.get<LifetimeTracker>();
  EXPECT_EQ(1, LifetimeTracker::numCtorCalls);
  EXPECT_EQ(0, LifetimeTracker::numCopyCalls);
  EXPECT_EQ(1, LifetimeTracker::numMoveCalls);

  // We use RVO throughout Injector::create, so only one ctor call should be made
  const auto l3 = inj.create<LifetimeTracker>();
  EXPECT_EQ(2, LifetimeTracker::numCtorCalls);
  EXPECT_EQ(0, LifetimeTracker::numCopyCalls);
  EXPECT_EQ(1, LifetimeTracker::numMoveCalls);  // create doesn't require a move
}

TEST_F(Injector_test, simpleRecipe) {
  Injector inj;

  int i = 0;

  constexpr int MAGIC = 42;

  inj.add_recipe<int>([&]([[maybe_unused]] Injector &injArg) {
    ++i;

    return MAGIC;
  });

  EXPECT_EQ(0, i) << "Injector::add_recipe should not eagerly invoke the recipe";

  {
    const int &newInstance = inj.get<int>();

    EXPECT_EQ(MAGIC, newInstance)
      << "The return value of a recipe should be used as the new instance of T";

    EXPECT_EQ(1, i) << "A recipe should be invoked when a new instance of T is requested";
  }

  {
    const int &refInt = inj.get<int>();

    EXPECT_EQ(42, refInt)
      << "The return value of a recipe should be cached when invoked via Injector::get";
    EXPECT_EQ(1, i) << "A recipe should not be invoked via Injector::get more than once";
  }

  {
    [[maybe_unused]] const int &newInt  = inj.create<int>();
    [[maybe_unused]] const int &newInt2 = inj.create<int>();

    EXPECT_EQ(3, i) << "Injector::create should invoke a recipe each time it is called";
  }
}

TEST_F(Injector_test, simpleCtorRecipe) {
  Injector inj;

  struct Klass {
    Klass(DepA &a, DepB b, DepC *c) : a_(a), b_(b), c_(c) { }

    DepA &a_;
    DepB  b_;
    DepC *c_;
  };

  inj.add_ctor_recipe<Klass, DepA &, DepB, DepC *>();

  [[maybe_unused]] const auto k = inj.create<Klass>();
  EXPECT_EQ(1, DepA::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for references";
  EXPECT_EQ(1, DepB::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::create for values";
  EXPECT_EQ(1, DepC::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for pointers";

  [[maybe_unused]] const auto &klassRef = inj.get<Klass>();
  EXPECT_EQ(1, DepA::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for references";
  EXPECT_EQ(2, DepB::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::create for values";
  EXPECT_EQ(1, DepC::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for pointers";

  [[maybe_unused]] const auto &klassRef2 = inj.get<Klass>();
  EXPECT_EQ(1, DepA::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for references";
  EXPECT_EQ(2, DepB::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::create for values";
  EXPECT_EQ(1, DepC::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for pointers";

  [[maybe_unused]] const auto k2 = inj.create<Klass>();
  EXPECT_EQ(1, DepA::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for references";
  EXPECT_EQ(3, DepB::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::create for values";
  EXPECT_EQ(1, DepC::instanceCounter)
    << "Injector::add_ctor_recipe should call Injector::get for pointers";
}

TEST_F(Injector_test, bind_impl) {
  class Base {
  public:
    virtual std::string_view get_str() { return "BASE"; }

    Base()                        = default;
    Base(const Base &)            = default;
    Base(Base &&)                 = default;
    Base &operator=(const Base &) = default;
    Base &operator=(Base &&)      = default;
    virtual ~Base()               = default;
  };

  class Derived : public Base {
  public:
    std::string_view get_str() override { return "DERIVED"; }
  };

  Injector inj;
  inj.bind_impl<Derived, Base>();

  {
    auto &base = inj.get<Base>();

    EXPECT_EQ("DERIVED", base.get_str())
      << "Injector::get<Base> should respect Injector::bind_impl recipes added for Base";

    EXPECT_THROW(inj.create<Base>(), std::runtime_error)
      << "Injector::create<Base> should throw if Injector::bind_impl added a recipe for Base";
  }

  {
    int i = 0;
    EXPECT_THROW(inj.add_recipe<Base>([&](Injector &) {
      ++i;
      return Base();
    }),
                 std::runtime_error)
      << "Injector::add_recipe<T> should throw if a bind_impl<T, U> was already invoked";
  }
}

TEST_F(Injector_test, bind_impl_abstract) {
  class Base {
  public:
    virtual std::string_view get_str() = 0;

    Base()                        = default;
    Base(const Base &)            = default;
    Base(Base &&)                 = default;
    Base &operator=(const Base &) = default;
    Base &operator=(Base &&)      = default;
    virtual ~Base()               = default;
  };

  class Derived : public Base {
  public:
    std::string_view get_str() override { return "DERIVED"; }
  };

  Injector inj;
  inj.bind_impl<Derived, Base>();

  {
    auto &base = inj.get<Base>();

    EXPECT_EQ("DERIVED", base.get_str()) << "Injector::get<AbstractBase> should respect "
                                            "Injector::bind_impl recipes added for AbstractBase";

    // N.B. that this won't compile b/c Base is an abstract type
    // auto base2 = inj.create<Base>();
    // EXPECT_EQ("BASE", base2.get_str())
    //   << "Injector::create<Base> should ignore Injector::bind_impl recipes added for Base";
  }

  // ditto
  // {
  //   int i = 0;
  //   inj.add_recipe<Base>([&](Injector &) {
  //     ++i;
  //     return Base();
  //   });
  //
  //   auto &base3 = inj.get<Base>();
  //   EXPECT_EQ("DERIVED", base3.get_str())
  //     << "If Injector::bind_impl<Base> has been invoked, then Injector::get<Base> should ignore a
  //     "
  //        "recipe added by Injector::add_recipe<Base>";
  //   EXPECT_EQ(0, i)
  //     << "If Injector::bind_impl<Base> has been invoked, then Injector::get<Base> should ignore a
  //     "
  //        "recipe added by Injector::add_recipe<Base>";
  // }
}

TEST_F(Injector_test, get_injector_ref) {
  Injector inj;
  auto    &injRef = inj.get<Injector>();
  EXPECT_EQ(&inj, &injRef)
    << "Injector::get<Injector>() should return a reference to the Injector instance";

  auto new_inj = inj.create<Injector>();
  EXPECT_NE(&inj, &new_inj) << "Injector::create<Injector>() should create a new Injector instance";
}

TEST_F(Injector_test, throw_when_no_abstract_recipe) {
  struct Iface {
    Iface()                         = default;
    Iface(const Iface &)            = default;
    Iface(Iface &&)                 = delete;
    Iface &operator=(const Iface &) = default;
    Iface &operator=(Iface &&)      = delete;
    virtual ~Iface()                = default;
    virtual void virt()             = 0;
  };

  Injector inj;
  EXPECT_THROW([[maybe_unused]] auto &ifaceRef = inj.get<Iface>(), std::runtime_error)
    << "Injector::get should throw when requesting an abstract type that has no recipe, even if "
       "that abstract type has a default constructor";
}

TEST_F(Injector_test, throw_on_dependency_cycle) {
  struct A;
  struct B;

  struct A {
    explicit A([[maybe_unused]] B &b) { };
  };

  struct B {
    explicit B([[maybe_unused]] A &a) { };
  };

  Injector inj;
  inj.add_ctor_recipe<A, B &>();
  inj.add_ctor_recipe<B, A &>();

  EXPECT_THROW([[maybe_unused]] auto &aRef = inj.get<A>(), std::runtime_error)
    << "Injector::get should throw when a dependency cycle is detected";

  EXPECT_THROW([[maybe_unused]] auto &bRef = inj.get<B>(), std::runtime_error)
    << "Injector::get should throw when a dependency cycle is detected";
}

TEST_F(Injector_test, throw_on_nested_dependency_cycle) {
  struct A;
  struct B;
  struct C;
  struct D;
  struct E;

  struct A {
    explicit A([[maybe_unused]] E &e) { };
  };

  struct B {
    explicit B([[maybe_unused]] A &a) { };
  };

  struct C {
    explicit C([[maybe_unused]] B &b) { };
  };

  struct D {
    explicit D([[maybe_unused]] C &c) { };
  };

  struct E {
    explicit E([[maybe_unused]] D &d) { };
  };

  Injector inj;
  inj.add_ctor_recipe<A, E &>();
  inj.add_ctor_recipe<B, A &>();
  inj.add_ctor_recipe<C, B &>();
  inj.add_ctor_recipe<D, C &>();
  inj.add_ctor_recipe<E, D &>();

  EXPECT_THROW([[maybe_unused]] auto &aRef = inj.get<A>(), std::runtime_error)
    << "Injector::get should throw when a dependency cycle is detected, even when it is nested";

  EXPECT_THROW([[maybe_unused]] auto &eRef = inj.get<E>(), std::runtime_error)
    << "Injector::get should throw when a dependency cycle is detected, even when it is nested";
}

TEST_F(Injector_test, throw_when_no_recipe_and_not_default_constructible) {
  struct Klass {
    explicit Klass(int i) : n(i) { }

    int n;
  };

  static_assert(!std::default_initializable<Klass>);

  Injector inj;
  EXPECT_THROW([[maybe_unused]] auto &klassRef = inj.get<Klass>(), std::runtime_error)
    << "Injector::get should throw when a type that is not default constructible is requested, and "
       "there is no recipe for it";

  inj.add_ctor_recipe<Klass, int>();
  EXPECT_NO_THROW([[maybe_unused]] auto &klassRef = inj.get<Klass>())
    << "Injector::get should not throw when a type that is not default constructible is requested, "
       "and there is a recipe for it";
}

TEST_F(Injector_test, delete_in_reverse_order_of_creation) {
  // NOLINTBEGIN
  struct A {
    ~A() { dtorTracker.emplace_back("A"); }
  };

  struct B {
    ~B() { dtorTracker.emplace_back("B"); }
  };

  struct C {
    ~C() { dtorTracker.emplace_back("C"); }
  };

  // NOLINTEND

  {
    Injector inj;

    {
      [[maybe_unused]] auto &c = inj.get<C>();
      [[maybe_unused]] auto &b = inj.get<B>();
      [[maybe_unused]] auto &a = inj.get<A>();
    }

    // Awkward, but we need to reset this, since the destructor will be called on the moved-from
    // instances when each type instance was created (the TypeMap::insert call in Injector::get)
    dtorTracker.clear();
  }

  EXPECT_EQ(3, dtorTracker.size());
  EXPECT_EQ("A", dtorTracker.at(0))
    << "On destruction, an Injector instance should delete the instances it owns in the reverse "
       "order in which they were created";
  EXPECT_EQ("B", dtorTracker.at(1))
    << "On destruction, an Injector instance should delete the instances it owns in the reverse "
       "order in which they were created";
  EXPECT_EQ("C", dtorTracker.at(2))
    << "On destruction, an Injector instance should delete the instances it owns in the reverse "
       "order in which they were created";
}
