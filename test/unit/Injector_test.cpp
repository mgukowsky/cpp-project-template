#include "mgfw/Injector.hpp"

#include <gtest/gtest.h>

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
  }
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

  const auto &l = inj.get<LifetimeTracker>();
  EXPECT_EQ(1, LifetimeTracker::numCtorCalls);
  EXPECT_EQ(0, LifetimeTracker::numCopyCalls);
  EXPECT_EQ(1, LifetimeTracker::numMoveCalls);  // one move is needed to place the entry into the
                                                // type container

  // No construction should be needed once the instance is cached
  const auto &l2 = inj.get<LifetimeTracker>();
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

  inj.add_recipe<int>([&]([[maybe_unused]] Injector &inj) {
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
    const int &newInt  = inj.create<int>();
    const int &newInt2 = inj.create<int>();

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
