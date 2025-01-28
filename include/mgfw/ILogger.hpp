#pragma once

#include "mgfw/types.hpp"

#include <string_view>

namespace mgfw {

/**
 * Logging class to abstract away the underlying log library. All logging
 * functions accept a libfmt-style series of arguments, ie. log("fmt string", args...)
 */

class ILogger {
public:
  enum class LogLevel : U8 {
    OFF,
    CRITICAL,
    ERR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
  };

  ILogger()          = default;
  virtual ~ILogger() = default;

  ILogger(const ILogger &)            = delete;
  ILogger &operator=(const ILogger &) = delete;
  ILogger(ILogger &&)                 = default;
  ILogger &operator=(ILogger &&)      = default;

  virtual void critical(std::string_view msg) = 0;
  virtual void error(std::string_view msg)    = 0;
  virtual void warn(std::string_view msg)     = 0;
  virtual void info(std::string_view msg)     = 0;
  virtual void debug(std::string_view msg)    = 0;
  virtual void trace(std::string_view msg)    = 0;

  /**
   * The remainder of the functions here serve to manipulate the state of the underlying
   * logging library.
   */
  virtual void set_level(LogLevel level) = 0;
};

}  // namespace mgfw
