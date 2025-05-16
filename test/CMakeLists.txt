cpmaddpackage(
  NAME
  googletest
  GITHUB_REPOSITORY
  google/googletest
  GIT_TAG
  v1.14.0
  VERSION
  v1.14.0
  OPTIONS
  "INSTALL_GTEST OFF"
  "gtest_force_shared_crt")

include(GoogleTest)

# Use to add tests TESTNAME must have a corresponding ${TESTNAME}_test.cpp, in
# addition to any other source files. From
# https://cliutils.gitlab.io/modern-cmake/chapters/testing/googletest.html
function(add_unit_test TESTNAME)
  add_executable(${TESTNAME})

  configure_target(${TESTNAME})

  # Add coverage
  target_compile_options(
    ${TESTNAME}
    PRIVATE
      --coverage
      # $<$<CXX_COMPILER_ID:GNU>:--coverage>
      # More powerful source-based coverage; disabled for now
      # $<$<CXX_COMPILER_ID:Clang>:-fprofile-instr-generate -fcoverage-mapping>
  )

  target_link_options(
    ${TESTNAME}
    PRIVATE
      --coverage
      # $<$<CXX_COMPILER_ID:GNU>:--coverage>
      # $<$<CXX_COMPILER_ID:Clang>:-fprofile-instr-generate -fcoverage-mapping>
  )

  target_include_directories(${TESTNAME} PUBLIC ${PROJECT_SOURCE_DIR}/test/include)
  target_link_libraries(${TESTNAME} gtest gmock gtest_main)

  target_sources(
    ${TESTNAME} PRIVATE ${PROJECT_SOURCE_DIR}/test/unit/${TESTNAME}_test.cpp
                        ${ARGN})

  gtest_discover_tests(
    ${TESTNAME}
    DISCOVERY_MODE
    PRE_TEST
    # Prevent slower CI systems from timing out. See
    # https://stackoverflow.com/questions/68706014/gtest-discover-tests-failing-in-github-actions-process-terminated-due-to-timeou
    DISCOVERY_TIMEOUT
    60)
endfunction()

# Examples of how to add tests
#   add_unit_test(smoke)
#   add_unit_test(gb_Bus ${PROJECT_SOURCE_DIR}/src/gb/Bus.cpp)
#   add_unit_test(gb_CPU ${PROJECT_SOURCE_DIR}/src/gb/CPU.cpp
              # ${PROJECT_SOURCE_DIR}/src/gb/Bus.cpp)

add_unit_test(defer)
add_unit_test(Injector ${PROJECT_SOURCE_DIR}/src/mgfw/Injector.cpp)
add_unit_test(events) # Tests MessageQueue, EventReader, EventWriter
add_unit_test(TypeHash)
add_unit_test(TypeMap)
add_unit_test(TypeString)
