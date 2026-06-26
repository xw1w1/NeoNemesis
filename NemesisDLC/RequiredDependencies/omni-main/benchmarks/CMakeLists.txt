message(STATUS "[OMNI] Configuring benchmarks")

include(OmniUseGoogleBenchmark)

set(omni_benchmark_fixture_directory "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
set(omni_benchmark_fixture_export_counts 16 64 1024 4096)
set(omni_benchmark_fixture_targets)

function(omni_add_benchmark_export_fixture export_count)
  set(fixture_source "${omni_benchmark_fixture_directory}/exports_${export_count}.cpp")
  set(fixture_target "omni_benchmark_exports_${export_count}")

  add_custom_command(
    OUTPUT "${fixture_source}"
    COMMAND "${CMAKE_COMMAND}"
      "-DOUTPUT_FILE:FILEPATH=${fixture_source}"
      "-DEXPORT_COUNT=${export_count}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_export_fixture.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generate_export_fixture.cmake"
    VERBATIM
  )

  add_library("${fixture_target}" SHARED "${fixture_source}")
  target_compile_features("${fixture_target}" PRIVATE cxx_std_23)
  set_target_properties("${fixture_target}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${omni_benchmark_fixture_directory}"
    LIBRARY_OUTPUT_DIRECTORY "${omni_benchmark_fixture_directory}"
    ARCHIVE_OUTPUT_DIRECTORY "${omni_benchmark_fixture_directory}"
  )
endfunction()

function(omni_add_benchmark source)
  get_filename_component(benchmark_name "${source}" NAME_WE)
  set(benchmark_target "omni_benchmark_${benchmark_name}")

  add_executable("${benchmark_target}" "${source}")
  omni_use_google_benchmark("${benchmark_target}")

  target_compile_features("${benchmark_target}" PRIVATE cxx_std_23)
  target_link_libraries("${benchmark_target}" PRIVATE omni::omni)
  target_compile_definitions("${benchmark_target}" PRIVATE OMNI_BENCHMARK_FIXTURE_DIRECTORY="${omni_benchmark_fixture_directory}")

  add_dependencies("${benchmark_target}" ${omni_benchmark_fixture_targets})
endfunction()

foreach(export_count IN LISTS omni_benchmark_fixture_export_counts)
  omni_add_benchmark_export_fixture("${export_count}")
  list(APPEND omni_benchmark_fixture_targets "omni_benchmark_exports_${export_count}")
endforeach()

file(GLOB omni_benchmark_sources CONFIGURE_DEPENDS *.cpp)

foreach(source IN LISTS omni_benchmark_sources)
  omni_add_benchmark("${source}")
endforeach()
