#include "mgfw/TypeString.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

class Klass { };

using mgfw::TypeString;

TEST(TypeString_test, getTypeStrings) {
  EXPECT_STREQ("int", std::string(TypeString<int>).c_str())
    << "TypeString<T> correctly generates a string with the name of T";
  EXPECT_STREQ("int", std::string(TypeString<int &>).c_str())
    << "TypeString<T> correctly generates a string with the name of T";
  EXPECT_STREQ("const int", std::string(TypeString<const int * const>).c_str())
    << "TypeString<T> correctly generates a string with the name of T";

// MSVC mangles class names a little bit differently than what these two tests check for, however
// the names of the types are still clear.
#if defined(__clang__) || defined(__GNUC__)
  EXPECT_STREQ("Klass", std::string(TypeString<Klass>).c_str())
    << "TypeString<T> correctly generates a string with the name of T";
  EXPECT_STREQ("std::vector<const Klass>",
               std::string(TypeString<const std::vector<Klass>>).c_str())
    << "TypeString<T> correctly generates a string with the name of T";
#endif
}
