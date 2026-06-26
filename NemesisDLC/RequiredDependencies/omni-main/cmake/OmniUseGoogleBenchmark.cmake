include_guard(GLOBAL)

include(FetchContent)

function(_omni_try_enable_google_benchmark dependent_target)
  if(NOT TARGET benchmark::benchmark)
    find_package(benchmark QUIET)
  endif()

  if(TARGET benchmark::benchmark)
    message(STATUS "[OMNI] Using Google benchmark (benchmark::benchmark)")
  endif()
endfunction()

function(_omni_enable_bundled_google_benchmark dependent_target)
  set(google_benchmark_source_dir "${CMAKE_BINARY_DIR}/_deps/google_benchmark-src")
  set(google_benchmark_binary_dir "${CMAKE_BINARY_DIR}/_deps/google_benchmark-build")

  if(EXISTS "${google_benchmark_source_dir}/CMakeLists.txt")
    message(STATUS "[OMNI] Using cached Google benchmark source at ${google_benchmark_source_dir}")
    add_subdirectory("${google_benchmark_source_dir}" "${google_benchmark_binary_dir}")
    return()
  endif()

  message(STATUS "[OMNI] Fetching Google benchmark v2.3.1 from git...")

  FetchContent_Declare(google_benchmark
    GIT_REPOSITORY  https://github.com/google/benchmark
    GIT_TAG         v1.9.5
    GIT_SHALLOW     ON
    UPDATE_DISCONNECTED ON
  )
  FetchContent_MakeAvailable(google_benchmark)
endfunction()

function(omni_use_google_benchmark dependent_target)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)

  if(NOT TARGET benchmark::benchmark)
    _omni_try_enable_google_benchmark(${dependent_target})

    if(NOT TARGET benchmark::benchmark)
      _omni_enable_bundled_google_benchmark(${dependent_target})
    endif()
  endif()

  target_link_libraries(${dependent_target} PRIVATE benchmark::benchmark)
endfunction()
