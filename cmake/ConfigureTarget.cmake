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
endfunction()
