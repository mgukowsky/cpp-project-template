cmake_minimum_required(VERSION 3.28)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

option(MYPROJ_BUILD_TESTS "Build test executables" ON)

project(
  MYPROJ
  VERSION 0.0.0
  LANGUAGES C CXX)

message(STATUS "Staring CMake with " ${CMAKE_CXX_COMPILER_ID} " on "
               ${CMAKE_SYSTEM_NAME})

include(Deps)

set(MYPROJ_LIBNAME "${PROJECT_NAME}_lib" CACHE INTERNAL "Project library")
set(MYPROJ_EXENAME "${PROJECT_NAME}" CACHE INTERNAL "The main executable")

add_library(${MYPROJ_LIBNAME} STATIC)

include(ConfigureTarget)
configure_target(${MYPROJ_LIBNAME})

include(SourceFileManifest)
target_sources(${MYPROJ_LIBNAME} PRIVATE ${MYPROJ_LIB_SOURCE_MANIFEST})

target_link_libraries(${MYPROJ_LIBNAME}
  PUBLIC
    ${CMAKE_DL_LIBS}
    concurrentqueue
    spdlog::spdlog
    Threads::Threads
    glfw
)

add_executable(${MYPROJ_EXENAME})
configure_target(${MYPROJ_EXENAME})
target_sources(${MYPROJ_EXENAME} PRIVATE ${MYPROJ_EXE_SOURCE_MANIFEST})
target_link_libraries(${MYPROJ_EXENAME}
  PUBLIC
    ${MYPROJ_LIBNAME}
)

if(${MYPROJ_BUILD_TESTS})
  include(CTest)
  enable_testing()
  add_subdirectory("test")
endif()
