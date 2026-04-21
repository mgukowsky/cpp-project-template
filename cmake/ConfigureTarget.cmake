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
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    config_for_msvc(${target_name})
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
      $<$<AND:$<CONFIG:Debug>,$<STREQUAL:${SANITIZER_TYPE},thread>>:-fsanitize=thread>
      $<$<AND:$<CONFIG:Debug>,$<NOT:$<STREQUAL:${SANITIZER_TYPE},thread>>>:-fsanitize=address,leak,undefined>)

  target_link_options(${target_name} PUBLIC
                      $<$<AND:$<CONFIG:Debug>,$<STREQUAL:${SANITIZER_TYPE},thread>>:-fsanitize=thread>
                      $<$<AND:$<CONFIG:Debug>,$<NOT:$<STREQUAL:${SANITIZER_TYPE},thread>>>:-fsanitize=address,leak,undefined>)

endfunction()

function(config_for_msvc target_name)
  target_compile_options(
    ${target_name}
    PUBLIC
      # High warning level and treat warnings as errors
      /W4
      /WX
      # Strict standards conformance
      /permissive-
      # Enable additional warnings that are off by default
      /w14242 # 'identifier': conversion from 'type1' to 'type2', possible loss of data
      /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
      /w14263 # 'function': member function does not override any base class virtual member function
      /w14265 # 'classname': class has virtual functions, but destructor is not virtual
      /w14287 # 'operator': unsigned/negative constant mismatch
      /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside the for-loop scope
      /w14296 # 'operator': expression is always 'boolean_value'
      /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
      /w14545 # expression before comma evaluates to a function which is missing an argument list
      /w14546 # function call before comma missing argument list
      /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
      /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
      /w14555 # expression has no effect; expected expression with side-effect
      /w14619 # pragma warning: there is no warning number 'number'
      /w14640 # Enable warning on thread un-safe static member initialization
      /w14826 # Conversion from 'type1' to 'type2' is sign-extended
      /w14905 # wide string literal cast to 'LPSTR'
      /w14906 # string literal cast to 'LPWSTR'
      /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
      # Standards conformance options
      /Zc:__cplusplus # Report correct C++ version macro
      /Zc:inline # Remove unreferenced COMDAT
      /Zc:preprocessor # Use conforming preprocessor
      /Zc:throwingNew # Assume operator new throws on failure
      # UTF-8 source and execution character sets
      /utf-8
      # Address sanitizer for debug builds (supported since VS 2019 16.9)
      $<$<CONFIG:Debug>:/fsanitize=address>)

  target_link_options(${target_name} PUBLIC
                      $<$<CONFIG:Debug>:/fsanitize=address>)

endfunction()
