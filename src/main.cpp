#include <mgfw/Clock.hpp>
#include <mgfw/Injector.hpp>
#include <mgfw/Scheduler.hpp>
#include <mgfw/SpdlogLogger.hpp>

#include <chrono>

using mgfw::Clock;
using mgfw::IClock;
using mgfw::ILogger;
using mgfw::Injector;
using mgfw::Scheduler;
using mgfw::SpdlogLogger;

using namespace std::chrono_literals;

int main([[maybe_unused]] const int argc, [[maybe_unused]] const char **argv) {
  Injector inj;
  inj.bind_impl<Clock, IClock>();
  inj.bind_impl<SpdlogLogger, ILogger>();

  inj.add_ctor_recipe<Scheduler, IClock &, ILogger &>();

  auto &logger = inj.get<ILogger>();
  auto &sched  = inj.get<Scheduler>();

  sched.set_interval(1s, [&] { logger.info("Hi!"); });

  sched.run();
}
