#include "mgfw/Window.hpp"

#include <GLFW/glfw3.h>

namespace mgfw {

struct Window::Impl_ {
  Impl_() { glfwInit(); }

  ~Impl_() { glfwTerminate(); }

  Impl_(const Impl_ &)            = delete;
  Impl_(Impl_ &&)                 = delete;
  Impl_ &operator=(const Impl_ &) = delete;
  Impl_ &operator=(Impl_ &&)      = delete;
};

}  // namespace mgfw
