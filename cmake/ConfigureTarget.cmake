# Sources: - https://github.com/TheLartians/ModernCppStarter -
# https://github.com/StableCoder/cmake-scripts -
# https://github.com/cpp-best-practices/cmake_template/tree/main -
# https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#compilers

# Standard CMake modules
include(CheckIPOSupported)
include(CheckPIESupported)

function(configure_target target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_23)
  set_target_properties(
    ${target_name}
    PROPERTIES CXX_STANDARD 23
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF
               MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)

  target_include_directories(${target_name}
                             PUBLIC ${PROJECT_SOURCE_DIR}/include)

  # Best practice to check for PIE before enabling it
  check_pie_supported()
  if(CMAKE_CXX_LINK_PIE_SUPPORTED)
    set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE
                                                    TRUE)
  endif()

  # Use IPO if Release and available
  if(CMAKE_BUILD_TYPE MATCHES Release)
    check_ipo_supported(RESULT is_ipo_supported)
    if(is_ipo_supported)
      set_target_properties(${target_name}
                            PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                            "Clang")
    config_for_unix(${target_name})
  endif()
endfunction()

function(config_for_unix target_name)
  target_compile_options(
    ${target_name}
    PUBLIC
      # recommended per
      # https://github.com/cpp-best-practices/cppbestpractices/blob/master/02-Use_the_Tools_Available.md#compilers
      -pedantic
      -Wall
      -Wcast-align
      -Wconversion
      -Wdouble-promotion
      -Werror
      -Wextra
      -Wformat=2
      -Wnull-dereference
      -Wnon-virtual-dtor
      -Wold-style-cast
      -Woverloaded-virtual
      -Wpedantic
      -Wshadow
      -Wsign-conversion
      -Wunused
      -Wwrite-strings
      $<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>
      $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-branches>
      $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond>
      $<$<CXX_COMPILER_ID:GNU>:-Wlogical-op>
      $<$<CXX_COMPILER_ID:GNU>:-Wmisleading-indentation>
      $<$<CXX_COMPILER_ID:GNU>:-Wsuggest-override>
      $<$<CXX_COMPILER_ID:GNU>:-Wuseless-cast>
      $<$<CXX_COMPILER_ID:Clang>:-fcolor-diagnostics>
      $<$<CXX_COMPILER_ID:Clang>:-Wimplicit-fallthrough>
      $<$<CXX_COMPILER_ID:Clang>:-Wno-string-conversion>
      $<$<CONFIG:Debug>:-fsanitize=address,leak,undefined>)

  target_link_options(${target_name} PUBLIC
                      $<$<CONFIG:Debug>:-fsanitize=address,leak,undefined>)

endfunction()
