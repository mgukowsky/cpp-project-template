#include "mgfw/CVar.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using mgfw::CVar;

// Construction and correct get
TEST(CVarTest, ConstructAndGetInt) {
  CVar cvar("test_int", 42, "desc");
  EXPECT_EQ(cvar.name(), "test_int");
  EXPECT_EQ(cvar.desc(), "desc");
  EXPECT_EQ(cvar.get<int>(), 42);
}

TEST(CVarTest, ConstructAndGetDouble) {
  CVar cvar("test_double", 3.1415, "desc");
  EXPECT_DOUBLE_EQ(cvar.get<double>(), 3.1415);
}

TEST(CVarTest, ConstructAndGetBool) {
  CVar cvar("test_bool", true, "desc");
  EXPECT_EQ(cvar.get<bool>(), true);
}

TEST(CVarTest, ConstructAndGetString) {
  CVar cvar("test_str", std::string("hello"), "desc");
  EXPECT_EQ(cvar.get<std::string>(), "hello");
}

// Correct set (same type)
TEST(CVarTest, SetSameType) {
  CVar cvar("test", 1);
  cvar.set<int>(42);
  EXPECT_EQ(cvar.get<int>(), 42);

  CVar cvar2("test2", std::string("foo"));
  cvar2.set<std::string>("bar");
  EXPECT_EQ(cvar2.get<std::string>(), "bar");
}

// Throws on get with wrong type
TEST(CVarTest, GetWrongTypeThrows) {
  CVar cvar("test", 42);
  EXPECT_THROW(cvar.get<std::string>(), std::runtime_error);
  EXPECT_THROW(cvar.get<double>(), std::runtime_error);
  EXPECT_THROW(cvar.get<bool>(), std::runtime_error);
}

// Throws on set with wrong type
TEST(CVarTest, SetWrongTypeThrows) {
  CVar cvar("test", 42);
  EXPECT_THROW(cvar.set<double>(3.14), std::runtime_error);
  EXPECT_THROW(cvar.set<std::string>("abc"), std::runtime_error);
  EXPECT_THROW(cvar.set<bool>(true), std::runtime_error);

  CVar cvar2("test2", std::string("foo"));
  EXPECT_THROW(cvar2.set<int>(7), std::runtime_error);
  EXPECT_THROW(cvar2.set<bool>(false), std::runtime_error);
}

// Throws on set after type change attempt
TEST(CVarTest, SetDoesNotChangeType) {
  CVar cvar("test", false);
  EXPECT_THROW(cvar.set<double>(2.0), std::runtime_error);
  EXPECT_EQ(cvar.get<bool>(), false);

  CVar cvar2("test2", 1.23);
  EXPECT_THROW(cvar2.set<int>(5), std::runtime_error);
  EXPECT_EQ(cvar2.get<double>(), 1.23);
}

// Throws with informative error message
TEST(CVarTest, ErrorMessageContainsNameAndType) {
  CVar cvar("myvar", 10, "desc");
  try {
    cvar.get<double>();
  }
  catch(const std::runtime_error &err) {
    std::string msg = err.what();
    EXPECT_NE(msg.find("myvar"), std::string::npos);
    EXPECT_NE(msg.find("double"), std::string::npos);
  }
}
