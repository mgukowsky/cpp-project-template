#include "mgfw/SyncCell.hpp"

#include <gtest/gtest.h>

#include <future>
#include <thread>

using mgfw::SyncCell;

TEST(SyncCellTest, UnlocksOnDestruction) {
  SyncCell<int> mtx(10);

  std::promise<void>       step1;
  std::shared_future<void> step1Future = step1.get_future().share();

  std::promise<void> step2;
  std::future<void>  step2Future = step2.get_future();

  std::atomic_bool t2_acquired = false;

  std::thread t1([&]() {
    auto lck = mtx.get_locked();
    step1.set_value();   // Let t2 know we've locked it
    step2Future.wait();  // Wait until main thread allows release
                         // lck destroyed here, mutex released
  });

  std::thread t2([&]() {
    step1Future.wait();              // Wait until t1 has locked
    auto lck    = mtx.get_locked();  // This should block until t1 releases
    t2_acquired = true;
  });

  // Ensure t1 has the lock
  step1Future.wait();

  // t2 is now blocked on acquiring the mutex
  EXPECT_FALSE(t2_acquired.load());

  // Release t1's lock
  step2.set_value();

  // Wait for t2 to finish acquiring
  t2.join();
  EXPECT_TRUE(t2_acquired.load());

  t1.join();
}
