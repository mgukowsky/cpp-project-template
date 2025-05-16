cmake_minimum_required(VERSION 3.28)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(GetCPM)

option(MYPROJ_BUILD_TESTS "Build test executables" ON)

project(
  MYPROJ
  VERSION 0.0.0
  LANGUAGES C CXX)

message(STATUS "Staring CMake with " ${CMAKE_CXX_COMPILER_ID} " on "
               ${CMAKE_SYSTEM_NAME})

add_executable(${PROJECT_NAME})

include(ConfigureTarget)
configure_target(${PROJECT_NAME})

find_package(Threads REQUIRED)

cpmaddpackage(
  NAME
  spdlog
  GITHUB_REPOSITORY
  gabime/spdlog
  GIT_TAG
  v1.15.3
  VERSION
  v1.15.3
  GIT_PROGRESS
  TRUE
)

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${CMAKE_DL_LIBS}
    spdlog::spdlog
    Threads::Threads
)

include(SourceFileManifest)
target_sources(${PROJECT_NAME} PRIVATE ${MYPROJ_SOURCE_FILE_MANIFEST})

if(MYPROJ_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory("test")
endif()
