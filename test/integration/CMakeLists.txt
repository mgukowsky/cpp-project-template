include(GoogleTest)

# Integration tests, designed to evaluate the interactions between components.
#
# Unlike unit tests, the integration tests are contained in a single executable that contains
# all the _actual_ code linked in (though mocks may still be used in places).
add_executable(integration)
configure_target(integration)
target_include_directories(integration PUBLIC ${PROJECT_SOURCE_DIR}/test/include)

include(SourceFileManifest)
target_sources(integration PRIVATE ${MYPROJ_INTEGRATION_SOURCE_MANIFEST})

target_link_libraries(integration
  PUBLIC
    ${MYPROJ_LIBNAME}
    gtest
    gmock
    gtest_main
)

gtest_discover_tests(
  integration
  DISCOVERY_MODE
  PRE_TEST
  DISCOVERY_TIMEOUT
  60)
