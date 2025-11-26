#include "mgfw/SpdlogLogger.hpp"

#include "mgfw/ILogger.hpp"
#include "spdlog/common.h"

#include <string_view>

#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <spdlog/fmt/bundled/color.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#if defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

using fmt::terminal_color;

namespace {
constexpr std::string_view FMTSTR = "{}";

auto colorize_string(const terminal_color color, const std::string_view &msg) {
  return fmt::format(fmt::fg(color), FMTSTR, msg);
}
}  // namespace

namespace mgfw {

SpdlogLogger::SpdlogLogger(const ILogger::LogLevel initialLevel)
  : logger_(spdlog::stdout_color_mt("app")) {
  // logger_->set_pattern("%D:%H:%M:%S:%f:%l : %v");
  SpdlogLogger::set_level(initialLevel);
}

SpdlogLogger::~SpdlogLogger() {
  if(logger_) {
    logger_->flush();
  }
}

// spdlog trips this lint check... but it's 3rd party and we're using the API correctly so we don't
// care

// NOLINTBEGIN(clang-analyzer-optin.cplusplus.UninitializedObject)
void SpdlogLogger::critical(std::string_view msg) {
  logger_->critical(FMTSTR, colorize_string(terminal_color::bright_magenta, msg));
}

void SpdlogLogger::error(std::string_view msg) {
  logger_->error(FMTSTR, colorize_string(terminal_color::bright_red, msg));
}

void SpdlogLogger::warn(std::string_view msg) {
  logger_->warn(FMTSTR, colorize_string(terminal_color::bright_yellow, msg));
}

void SpdlogLogger::info(std::string_view msg) {
  logger_->info(FMTSTR, colorize_string(terminal_color::bright_cyan, msg));
}

void SpdlogLogger::debug(std::string_view msg) {
  logger_->debug(FMTSTR, colorize_string(terminal_color::bright_white, msg));
}

void SpdlogLogger::trace([[maybe_unused]] std::string_view msg) {
  logger_->trace(FMTSTR, colorize_string(terminal_color::white, msg));
}

// NOLINTEND(clang-analyzer-optin.cplusplus.UninitializedObject)

void SpdlogLogger::set_level(ILogger::LogLevel level) {
  switch(level) {
    case ILogger::LogLevel::OFF:
      logger_->set_level(spdlog::level::off);
      break;

    case ILogger::LogLevel::CRITICAL:
      logger_->set_level(spdlog::level::critical);
      break;

    case ILogger::LogLevel::ERR:
      logger_->set_level(spdlog::level::err);
      break;

    case ILogger::LogLevel::WARN:
      logger_->set_level(spdlog::level::warn);
      break;

    case ILogger::LogLevel::INFO:
      logger_->set_level(spdlog::level::info);
      break;

    case ILogger::LogLevel::DEBUG:
      logger_->set_level(spdlog::level::debug);
      break;

    case ILogger::LogLevel::TRACE:
      logger_->set_level(spdlog::level::trace);
      break;

    default:
      break;
  }
}

}  // namespace mgfw
