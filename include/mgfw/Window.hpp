#pragma once

#include "mgfw/ILogger.hpp"

#include <memory>

namespace mgfw {

class Window {
public:
  explicit Window(ILogger &logger);
  ~Window();

  Window(const Window &)            = delete;
  Window &operator=(const Window &) = delete;

  Window(Window &&) noexcept;
  Window &operator=(Window &&) noexcept;

private:
  struct Impl_;
  std::unique_ptr<Impl_> impl_;

  ILogger &logger_;
};

}  // namespace mgfw
