#pragma once

#include "mgfw/ILogger.hpp"

#include <spdlog/spdlog.h>

#include <memory>

namespace mgfw {
class SpdlogLogger : public ILogger {
public:
  explicit SpdlogLogger(LogLevel initialLevel = LogLevel::INFO);

  ~SpdlogLogger() override;
  SpdlogLogger(const SpdlogLogger &)            = delete;
  SpdlogLogger &operator=(const SpdlogLogger &) = delete;
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
