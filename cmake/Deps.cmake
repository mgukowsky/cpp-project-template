include(GetCPM)

find_package(Threads REQUIRED)

cpmaddpackage(
  NAME
  concurrentqueue
  GITHUB_REPOSITORY
  cameron314/concurrentqueue
  GIT_TAG
  c68072129c8a5b4025122ca5a0c82ab14b30cb03
  VERSION
  v1.0.4
  GIT_PROGRESS
  TRUE
)

cpmaddpackage(
  NAME
  glfw
  GITHUB_REPOSITORY
  glfw/glfw
  GIT_TAG
  3.4
  VERSION
  3.4
  GIT_PROGRESS
  TRUE
  OPTIONS
    "GLFW_BUILD_DOCS OFF"
    "GLFW_BUILD_TESTS OFF"
    "GLFW_BUILD_EXAMPLES OFF"
)

cpmaddpackage(
  NAME
  spdlog
  GITHUB_REPOSITORY
  gabime/spdlog
  GIT_TAG
  v1.16.0
  VERSION
  v1.16.0
  GIT_PROGRESS
  TRUE
)

