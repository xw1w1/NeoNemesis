set(_res_blobs "")
set(_res_table "")

file(GLOB _res_files "${RES_DIR}/*")
list(SORT _res_files)

foreach(_f ${_res_files})
    if(IS_DIRECTORY "${_f}")
        continue()
    endif()

    get_filename_component(_name "${_f}" NAME)
    get_filename_component(_ext "${_f}" EXT)

    if(_ext STREQUAL ".cmake" OR _ext STREQUAL ".txt" OR _ext STREQUAL ".md")
        continue()
    endif()

    string(MAKE_C_IDENTIFIER "blob_${_name}" _sym)

    file(READ "${_f}" _hex HEX)
    string(LENGTH "${_hex}" _hexlen)
    math(EXPR _bytelen "${_hexlen} / 2")
    string(REGEX REPLACE "(..)" "0x\\1," _arr "${_hex}")

    string(APPEND _res_blobs "static const unsigned char ${_sym}[] = {${_arr}};\n")
    string(APPEND _res_table "    { \"${_name}\", ${_sym}, ${_bytelen}u },\n")
endforeach()

set(_out "#include \"embedded_resources.h\"\n")
string(APPEND _out "#include <cstring>\n\n")
string(APPEND _out "namespace {\n")
string(APPEND _out "${_res_blobs}")
string(APPEND _out "struct ResEntry { const char* name; const unsigned char* data; unsigned int size; };\n")
string(APPEND _out "static const ResEntry g_res_entries[] = {\n${_res_table}};\n")
string(APPEND _out "}\n\n")
string(APPEND _out "namespace Nemesis { namespace Res {\n")
string(APPEND _out "Blob Get(const char* name) {\n")
string(APPEND _out "    if (!name) return { nullptr, 0u };\n")
string(APPEND _out "    for (const auto& e : g_res_entries)\n")
string(APPEND _out "        if (std::strcmp(e.name, name) == 0) return { e.data, e.size };\n")
string(APPEND _out "    return { nullptr, 0u };\n")
string(APPEND _out "}\n")
string(APPEND _out "unsigned int Count() { return (unsigned int)(sizeof(g_res_entries) / sizeof(g_res_entries[0])); }\n")
string(APPEND _out "const char* NameAt(unsigned int i) { return i < Count() ? g_res_entries[i].name : nullptr; }\n")
string(APPEND _out "}}\n")

file(WRITE "${OUT_CPP}" "${_out}")
