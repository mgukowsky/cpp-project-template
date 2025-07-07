#pragma once

#include "mgfw/ILogger.hpp"

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <spdlog/spdlog.h>
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

#include <memory>

namespace mgfw {
class SpdlogLogger : public ILogger {
public:
  explicit SpdlogLogger(LogLevel initialLevel = LogLevel::INFO);

  ~SpdlogLogger() override;
  SpdlogLogger(const SpdlogLogger &)            = default;
  SpdlogLogger &operator=(const SpdlogLogger &) = default;
  SpdlogLogger(SpdlogLogger &&)                 = default;
  SpdlogLogger &operator=(SpdlogLogger &&)      = default;

  void critical(std::string_view msg) override;
  void error(std::string_view msg) override;
  void warn(std::string_view msg) override;
  void info(std::string_view msg) override;
  void debug(std::string_view msg) override;
  void trace(std::string_view msg) override;

  void set_level(LogLevel level) override;

private:
  std::shared_ptr<spdlog::logger> logger_;
};
}  // namespace mgfw
