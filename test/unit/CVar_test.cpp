#include "mgfw/CVar.hpp"

#include <gtest/gtest.h>

#include <numbers>
#include <string>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

using mgfw::CVar;

// Construction and correct get
TEST(CVarTest, ConstructAndGetInt) {
  const CVar<int> cvar("test_int", 42, "desc");
  EXPECT_EQ(cvar.name(), "test_int");
  EXPECT_EQ(cvar.desc(), "desc");
  EXPECT_EQ(cvar.get(), 42);
}

TEST(CVarTest, ConstructAndGetDouble) {
  const CVar<double> cvar("test_double", std::numbers::pi, "desc");
  EXPECT_DOUBLE_EQ(cvar.get(), std::numbers::pi);
}

TEST(CVarTest, ConstructAndGetBool) {
  const CVar<bool> cvar("test_bool", true, "desc");
  EXPECT_EQ(cvar.get(), true);
}

TEST(CVarTest, ConstructAndGetString) {
  const CVar<std::string> cvar("test_str", std::string("hello"), "desc");
  EXPECT_EQ(cvar.get(), "hello");
}

// Correct set (same type)
TEST(CVarTest, SetSameType) {
  CVar<int> cvar("test", 1);
  cvar.set(42);
  EXPECT_EQ(cvar.get(), 42);

  CVar<std::string> cvar2("test2", std::string("foo"));
  cvar2.set("bar");
  EXPECT_EQ(cvar2.get(), "bar");
}

TEST(CVarTest, SetInvokesCallbacks) {
  CVar<int> cvar("test", 1);
  int       i = 0;
  cvar.on_change([&](const int &prevVal, const int &newVal) {
    EXPECT_EQ(1, prevVal);
    EXPECT_EQ(42, newVal);
    i = 1234;
  });
  cvar.set(42);
  EXPECT_EQ(cvar.get(), 42);
  EXPECT_EQ(1234, i);
}

TEST(CVarTest, SkipCallbackIfValueIsUnchanged) {
  CVar<int> cvar("test", 1);
  int       i = 0;
  cvar.on_change(
    [&]([[maybe_unused]] const int &prevVal, [[maybe_unused]] const int &newVal) { i += 1000; });
  cvar.set(42);
  EXPECT_EQ(1000, i);

  cvar.set(84);
  EXPECT_EQ(2000, i);

  cvar.set(84);
  EXPECT_EQ(2000, i)
    << "on_change callbacks should be skipped if the value does not actually change";
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
