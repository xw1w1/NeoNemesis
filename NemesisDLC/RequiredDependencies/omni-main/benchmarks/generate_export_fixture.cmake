get_filename_component(output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

file(WRITE "${OUTPUT_FILE}" "#include <cstdint>\n\n")

function(omni_format_export_index value output_variable)
  set(formatted_value "${value}")

  string(LENGTH "${formatted_value}" formatted_value_length)

  while(formatted_value_length LESS 4)
    set(formatted_value "0${formatted_value}")
    string(LENGTH "${formatted_value}" formatted_value_length)
  endwhile()

  set("${output_variable}" "${formatted_value}" PARENT_SCOPE)
endfunction()

math(EXPR last_export_index "${EXPORT_COUNT} - 1")

foreach(export_index RANGE 0 ${last_export_index})
  # This keeps generated export names lexicographically ordered
  # by padding indices to 4 digits (0001, 0002...)
  omni_format_export_index("${export_index}" export_name_index)

  # Generate mock DLLs with controlled export table size, to keep
  # benchmark inputs stable and independent from system DLLs
  file(APPEND "${OUTPUT_FILE}" "extern \"C\" __declspec(dllexport) std::uint32_t omni_bm_export_${export_name_index}() { return ${export_index}u; }\n")
endforeach()
