#include <mgfw/SpdlogLogger.hpp>

int main([[maybe_unused]] const int argc, [[maybe_unused]] const char **argv) {
  mgfw::SpdlogLogger logger{};

  logger.info("hi");
  logger.warn("bye");
  logger.critical("aah");

  return 0;
}
