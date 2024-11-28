#include "mgfw/defer.hpp"

#include <gtest/gtest.h>

TEST(defer_test, deferTest) {
  int i = 3;

  {
    mgfw::defer deferred_([&]() { ++i; });

    EXPECT_EQ(3, i)
      << "A deferred function should not be executed until its container goes out of scope";
  }

  EXPECT_EQ(4, i) << "A deferred function should be executed once its container goes out of scope";
}
