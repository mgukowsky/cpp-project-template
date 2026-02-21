#include "mgfw/Injector.hpp"

#include "mgfw/TypeMap.hpp"

#include <gtest/gtest.h>

#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using mgfw::Injector;
using mgfw::TypeMap;

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

  inj.add_recipe<int>([&](Injector &, const TypeMap::InstanceId_t) {
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

TEST_F(Injector_test, specificInstanceCtorRecipe) {
  Injector inj;

  struct Dep { };

  struct S1 {
    explicit S1(Dep &dep) : dep_(dep) { }

    Dep &dep_;
  };

  struct S2 {
    explicit S2(Dep &dep) : dep_(dep) { }

    Dep &dep_;
  };

  struct S3 {
    explicit S3(Dep &dep) : dep_(dep) { }

    Dep &dep_;
  };

  inj.add_ctor_recipe<S1, Injector::Token<Dep, 1>>();
  inj.add_ctor_recipe<S2, Injector::Token<Dep, 2>>();
  inj.add_ctor_recipe<S3, Injector::Token<Dep, 1>>();

  const auto &s1ref = inj.get<S1>();
  const auto &s2ref = inj.get<S2>();
  const auto &s3ref = inj.get<S3>();

  EXPECT_NE(&(s1ref.dep_), &(s2ref.dep_));
  EXPECT_EQ(&(s1ref.dep_), &(s3ref.dep_));
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
    EXPECT_THROW(inj.add_recipe<Base>([&](Injector &, const TypeMap::InstanceId_t) {
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

TEST_F(Injector_test, createShouldSupportNonMoveableTypes) {
  Injector inj;

  constexpr auto MAGIC = 42;

  struct NoMoveNoCopy {
    explicit NoMoveNoCopy() = default;

    ~NoMoveNoCopy()                               = default;
    NoMoveNoCopy(const NoMoveNoCopy &)            = delete;
    NoMoveNoCopy(NoMoveNoCopy &&)                 = delete;
    NoMoveNoCopy &operator=(const NoMoveNoCopy &) = delete;
    NoMoveNoCopy &operator=(NoMoveNoCopy &&)      = delete;

    int i_ = MAGIC;
  };

  // If we used inj.get<NoMoveNoCopy>(), then this wouldn't compile, since that function needs to
  // perform a move and thus the type needs to be move constructible. Create uses RVO, so any type
  // would be valid!
  auto nmnc = inj.create<NoMoveNoCopy>();
  EXPECT_EQ(MAGIC, nmnc.i_);
}

TEST_F(Injector_test, addRecipeAndCreateShouldSupportNonMoveableTypes) {
  Injector inj;

  constexpr auto MAGIC = 42;

  class NoMoveNoCopy {
  public:
    explicit NoMoveNoCopy([[maybe_unused]] int i) { }

    ~NoMoveNoCopy()                               = default;
    NoMoveNoCopy(const NoMoveNoCopy &)            = delete;
    NoMoveNoCopy(NoMoveNoCopy &&)                 = delete;
    NoMoveNoCopy &operator=(const NoMoveNoCopy &) = delete;
    NoMoveNoCopy &operator=(NoMoveNoCopy &&)      = delete;

    int i_ = MAGIC;
  };

  inj.add_ctor_recipe<NoMoveNoCopy, int>();

  auto nmnc = inj.create<NoMoveNoCopy>();
  EXPECT_EQ(MAGIC, nmnc.i_);
}

TEST_F(Injector_test, instanceIds) {
  class DefaultCtorClass { };

  Injector inj;

  enum class IDs : mgfw::U8 { A, B };

  const auto &defref = inj.get<DefaultCtorClass>();
  const auto &aref1  = inj.get<DefaultCtorClass>(std::to_underlying(IDs::A));
  const auto &aref2  = inj.get<DefaultCtorClass>(std::to_underlying(IDs::A));
  const auto &bref   = inj.get<DefaultCtorClass>(std::to_underlying(IDs::B));

  // Default ref is -1 so it doesn't overlap with the first member of an enum!
  EXPECT_NE(&defref, &aref1);
  EXPECT_NE(&defref, &aref2);
  EXPECT_NE(&defref, &bref);

  // Instances w/ diff IDs should be distint
  EXPECT_NE(&aref1, &bref);
  EXPECT_NE(&aref2, &bref);

  // Same ID should be the same
  EXPECT_EQ(&aref1, &aref2);
}

TEST_F(Injector_test, bind_impl_with_instanceIds) {
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

  auto &defrefbase = inj.get<Base>();
  auto &ref0baseA  = inj.get<Base>(0);
  auto &ref0baseB  = inj.get<Base>(0);

  // Default instance != instance 0!
  EXPECT_NE(&defrefbase, &ref0baseA);
  EXPECT_NE(&defrefbase, &ref0baseB);

  // Same ID should yield same instance
  EXPECT_EQ(&ref0baseA, &ref0baseB);

  EXPECT_EQ("DERIVED", defrefbase.get_str())
    << "Injector::get<Base> should respect Injector::bind_impl recipes added for Base";

  // b/c the impl is bound to the iface, the iface and derived references should be the same, and
  // respect instance Ids!
  auto &defrefderived = inj.get<Derived>();
  auto &ref0derived   = inj.get<Derived>(0);
  EXPECT_EQ(&defrefbase, &defrefderived);
  EXPECT_NE(&defrefderived, &ref0derived);
  EXPECT_EQ(&ref0derived, &ref0baseA);
}

TEST_F(Injector_test, has_instance) {
  class DefaultCtorClass { };

  Injector inj;

  EXPECT_FALSE(inj.has_instance<DefaultCtorClass>())
    << "has_instance should return false before any instance is created";

  [[maybe_unused]] const auto &ref = inj.get<DefaultCtorClass>();

  EXPECT_TRUE(inj.has_instance<DefaultCtorClass>())
    << "has_instance should return true after get<T>() creates a cached instance";

  // create() does NOT cache, so has_instance should not be affected
  [[maybe_unused]] const auto val = inj.create<DefaultCtorClass>();
  EXPECT_TRUE(inj.has_instance<DefaultCtorClass>())
    << "create() should not affect the result of has_instance";

  // Instance IDs are independent
  enum class IDs : mgfw::U8 { A, B };

  EXPECT_FALSE(inj.has_instance<DefaultCtorClass>(std::to_underlying(IDs::A)))
    << "has_instance should return false for a named instance before it is created";

  [[maybe_unused]] const auto &aref = inj.get<DefaultCtorClass>(std::to_underlying(IDs::A));

  EXPECT_TRUE(inj.has_instance<DefaultCtorClass>(std::to_underlying(IDs::A)))
    << "has_instance should return true for a named instance after it is created";
  EXPECT_FALSE(inj.has_instance<DefaultCtorClass>(std::to_underlying(IDs::B)))
    << "has_instance for a different ID should still return false";
}

TEST_F(Injector_test, override_recipe) {
  Injector inj;

  constexpr int FIRST_MAGIC  = 42;
  constexpr int SECOND_MAGIC = 99;

  inj.add_recipe<int>([&](Injector &, const TypeMap::InstanceId_t) { return FIRST_MAGIC; });

  // override_recipe must not throw even when a recipe already exists
  EXPECT_NO_THROW(inj.override_recipe<int>(
    [&](Injector &, const TypeMap::InstanceId_t) { return SECOND_MAGIC; }))
    << "override_recipe should not throw when replacing an existing recipe";

  // override_recipe must not throw when no prior recipe exists either
  EXPECT_NO_THROW(
    inj.override_recipe<double>([&](Injector &, const TypeMap::InstanceId_t) { return 3.14; }))
    << "override_recipe should not throw when adding a recipe for the first time";

  const int &cached = inj.get<int>();
  EXPECT_EQ(SECOND_MAGIC, cached)
    << "get<T>() should use the recipe installed by override_recipe";

  // Calling create() also goes through the recipe
  const int fresh = inj.create<int>();
  EXPECT_EQ(SECOND_MAGIC, fresh)
    << "create<T>() should use the recipe installed by override_recipe";
}

TEST_F(Injector_test, override_ctor_recipe) {
  Injector inj;

  struct Klass {
    explicit Klass(DepA &a) : a_(a) { }

    DepA &a_;
  };

  inj.add_ctor_recipe<Klass, DepA &>();

  // Overriding with the same ctor recipe should not throw.
  // N.B. we must hoist the call into a lambda variable first because the comma in the template
  // argument list would otherwise be misinterpreted as a second macro argument.
  auto doOverride = [&] { inj.override_ctor_recipe<Klass, DepA &>(); };
  EXPECT_NO_THROW(doOverride())
    << "override_ctor_recipe should not throw when replacing an existing ctor recipe";

  [[maybe_unused]] const auto &k = inj.get<Klass>();
  EXPECT_EQ(1, DepA::instanceCounter)
    << "override_ctor_recipe should produce a fully functional recipe";
}

// ---------------------------------------------------------------------------
// Duplicate-registration error cases
// ---------------------------------------------------------------------------

TEST_F(Injector_test, throw_on_duplicate_add_recipe) {
  Injector inj;
  inj.add_recipe<int>([](Injector &, const TypeMap::InstanceId_t) { return 1; });

  EXPECT_THROW(inj.add_recipe<int>([](Injector &, const TypeMap::InstanceId_t) { return 2; }),
               std::runtime_error)
    << "add_recipe should throw when called a second time for the same type";
}

TEST_F(Injector_test, throw_on_duplicate_add_ctor_recipe) {
  Injector inj;
  inj.add_ctor_recipe<DepA>();

  EXPECT_THROW(inj.add_ctor_recipe<DepA>(), std::runtime_error)
    << "add_ctor_recipe should throw when called a second time for the same type";
}

TEST_F(Injector_test, throw_on_duplicate_bind_impl) {
  class Base {
  public:
    virtual ~Base() = default;
  };

  class Derived : public Base { };

  Injector inj;

  // N.B. wrap in a lambda to avoid the preprocessor misinterpreting the comma in <Derived, Base>
  // as a second macro argument.
  auto doFirstBind  = [&] { inj.bind_impl<Derived, Base>(); };
  auto doSecondBind = [&] { inj.bind_impl<Derived, Base>(); };

  doFirstBind();
  EXPECT_THROW(doSecondBind(), std::runtime_error)
    << "bind_impl should throw when called a second time for the same interface";
}

TEST_F(Injector_test, bind_impl_throws_if_recipe_already_added) {
  class Base {
  public:
    virtual ~Base()               = default;
    Base()                        = default;
    Base(const Base &)            = default;
    Base(Base &&)                 = default;
    Base &operator=(const Base &) = default;
    Base &operator=(Base &&)      = default;
  };

  class Derived : public Base { };

  Injector inj;
  inj.add_recipe<Base>([](Injector &, const TypeMap::InstanceId_t) { return Base(); });

  auto doBind = [&] { inj.bind_impl<Derived, Base>(); };
  EXPECT_THROW(doBind(), std::runtime_error)
    << "bind_impl should throw if add_recipe was already called for the same interface type";
}

// ---------------------------------------------------------------------------
// Transitive dependency resolution
// ---------------------------------------------------------------------------

TEST_F(Injector_test, transitive_dependencies) {
  // A -> B -> C; none of them are registered, so C is default-constructed first,
  // then B, then A – all via ctor recipes.
  struct C { };

  struct B {
    explicit B(C &c) : c_(c) { }

    C &c_;
  };

  struct A {
    explicit A(B &b) : b_(b) { }

    B &b_;
  };

  Injector inj;
  inj.add_ctor_recipe<A, B &>();
  inj.add_ctor_recipe<B, C &>();

  auto &a = inj.get<A>();
  auto &b = inj.get<B>();
  auto &c = inj.get<C>();

  EXPECT_EQ(&a.b_, &b)
    << "A should hold a reference to the singleton B resolved by the injector";
  EXPECT_EQ(&b.c_, &c)
    << "B should hold a reference to the singleton C resolved by the injector";
}

// ---------------------------------------------------------------------------
// instanceId is forwarded to the recipe function
// ---------------------------------------------------------------------------

TEST_F(Injector_test, recipe_receives_instance_id) {
  Injector inj;

  TypeMap::InstanceId_t capturedId = TypeMap::DEFAULT_INSTANCE_ID;

  inj.add_recipe<int>([&](Injector &, const TypeMap::InstanceId_t id) {
    capturedId = id;
    return 42;
  });

  constexpr TypeMap::InstanceId_t CUSTOM_ID = 7;

  [[maybe_unused]] auto &ref1 = inj.get<int>(CUSTOM_ID);
  EXPECT_EQ(CUSTOM_ID, capturedId)
    << "The recipe should receive the instanceId that was passed to get<T>(id)";

  // A fresh instance ID -> the recipe is invoked again (no cache hit)
  [[maybe_unused]] auto &ref2 = inj.get<int>();
  EXPECT_EQ(TypeMap::DEFAULT_INSTANCE_ID, capturedId)
    << "The recipe should receive DEFAULT_INSTANCE_ID when get<T>() is called without an id";
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST_F(Injector_test, injector_move) {
  class SimpleClass { };

  const SimpleClass *rawPtr = nullptr;

  Injector inj;
  inj.add_recipe<int>([](Injector &, const TypeMap::InstanceId_t) { return 99; });
  {
    auto &ref = inj.get<SimpleClass>();
    rawPtr    = &ref;
  }

  Injector inj2 = std::move(inj);

  EXPECT_TRUE(inj2.has_instance<SimpleClass>())
    << "Moved-to injector should see the instances that were cached before the move";

  EXPECT_EQ(rawPtr, &inj2.get<SimpleClass>())
    << "Moved-to injector should return the exact same object (same address)";

  // Recipes are also transferred
  EXPECT_EQ(99, inj2.create<int>())
    << "Moved-to injector should still have the recipes that were registered before the move";
}

// ---------------------------------------------------------------------------
// override_recipe interaction with the instance cache
// ---------------------------------------------------------------------------

TEST_F(Injector_test, override_recipe_does_not_affect_cached_instances) {
  Injector inj;

  constexpr int FIRST_MAGIC  = 42;
  constexpr int SECOND_MAGIC = 99;

  inj.add_recipe<int>([](Injector &, const TypeMap::InstanceId_t) { return FIRST_MAGIC; });

  // Cache the instance produced by the first recipe
  const int &cached = inj.get<int>();
  EXPECT_EQ(FIRST_MAGIC, cached);

  // Replace the recipe AFTER caching
  inj.override_recipe<int>([](Injector &, const TypeMap::InstanceId_t) { return SECOND_MAGIC; });

  // get<T>() must return the already-cached value – the new recipe is NOT re-invoked
  const int &ref2 = inj.get<int>();
  EXPECT_EQ(FIRST_MAGIC, ref2)
    << "After override_recipe, get<T>() should still return the previously cached instance";
  EXPECT_EQ(&cached, &ref2) << "The returned reference must be the same object";

  // create<T>() bypasses the cache and should use the new recipe
  EXPECT_EQ(SECOND_MAGIC, inj.create<int>())
    << "After override_recipe, create<T>() should use the new recipe";
}

TEST_F(Injector_test, override_recipe_replaces_bind_impl_recipe) {
  class Base {
  public:
    virtual std::string_view get_str() { return "BASE"; }

    virtual ~Base()               = default;
    Base()                        = default;
    Base(const Base &)            = default;
    Base(Base &&)                 = default;
    Base &operator=(const Base &) = default;
    Base &operator=(Base &&)      = default;
  };

  class Derived : public Base {
  public:
    std::string_view get_str() override { return "DERIVED"; }
  };

  Injector inj;
  auto     doBind = [&] { inj.bind_impl<Derived, Base>(); };
  doBind();

  // Replace the INTERFACE recipe installed by bind_impl with a CONCRETE one
  inj.override_recipe<Base>([](Injector &, const TypeMap::InstanceId_t) { return Base(); });

  // Now get<Base>() should construct a plain Base object via the concrete recipe
  auto &base = inj.get<Base>();
  EXPECT_EQ("BASE", base.get_str())
    << "After override_recipe replaces the bind_impl recipe, get<Base>() should use the "
       "concrete recipe and return a Base, not a Derived";
}

// ---------------------------------------------------------------------------
// has_instance edge cases
// ---------------------------------------------------------------------------

TEST_F(Injector_test, has_instance_returns_false_for_injector_self) {
  Injector inj;

  // get<Injector>() is a special identity case that returns *this without touching the TypeMap
  [[maybe_unused]] auto &injRef = inj.get<Injector>();

  EXPECT_FALSE(inj.has_instance<Injector>())
    << "get<Injector>() bypasses the TypeMap, so has_instance<Injector>() should be false";
}
