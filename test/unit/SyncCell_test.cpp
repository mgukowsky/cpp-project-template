#include "mgfw/SyncCell.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

using mgfw::SyncCell;

TEST(SyncCellTest, IsCondvarConcept) {
  {
    // gtest seems to be unhappy with us passing the concept directly into EXPECT_*
    const auto conceptResult = mgfw::
      is_condvar<std::condition_variable, std::unique_lock<std::mutex>, std::function<bool(void)>>;
    EXPECT_TRUE(conceptResult);
  }

  {
    const auto conceptResult =
      mgfw::is_condvar<std::condition_variable_any, std::mutex, std::function<bool(void)>>;
    EXPECT_TRUE(conceptResult);
  }

  {
    // Invalid CV type
    const auto conceptResult = mgfw::is_condvar<int, std::mutex, std::function<bool(void)>>;
    EXPECT_FALSE(conceptResult);
  }

  {
    // Invalid predicate type
    const auto conceptResult =
      mgfw::is_condvar<std::condition_variable, std::mutex, std::function<void(void)>>;
    EXPECT_FALSE(conceptResult);
  }
}

TEST(SyncCellTest, ScopedProxyLockTypeProperties) {
  using ProxyLock_t = SyncCell<int>::ScopedProxyLock;

  EXPECT_FALSE(std::copy_constructible<ProxyLock_t>)
    << "ScopedProxyLock should not be copyable b/c its contents shouldn't be able to escape the "
       "scope where it is created";

  EXPECT_FALSE(std::move_constructible<ProxyLock_t>)
    << "ScopedProxyLock should not be moveable b/c its contents shouldn't be able to escape the "
       "scope where it is created";
}

TEST(SyncCellTest, UnlocksOnDestruction) {
  // NOLINTNEXTLINE
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

TEST(SyncCellTest, transact) {
  struct S {
    int   i;
    float f;
  };

  constexpr int   INIT_INT   = 123;
  constexpr int   NEXT_INT   = 678;
  constexpr float INIT_FLOAT = 4.5;

  SyncCell<S> syncCell{INIT_INT, INIT_FLOAT};

  // N.B. that different return types for `transact` can be deduced based on the callable that's
  // passed in
  EXPECT_EQ(INIT_INT, syncCell.transact([](S &s) -> int { return s.i; }));
  EXPECT_EQ(INIT_FLOAT, syncCell.transact([](S &s) { return s.f; }));

  // Mutate state, and works with a func that returns void
  syncCell.transact([](S &s) { s.i = NEXT_INT; });
  EXPECT_EQ(NEXT_INT, syncCell.transact([](S &s) -> int { return s.i; }));

  // Funcs that return a ref won't compile
  // int &ri = syncCell.transact([](S &s) -> int & { return s.i; });
}
