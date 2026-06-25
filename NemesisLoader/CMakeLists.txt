cmake_minimum_required(VERSION 3.13)
project(NemesisLoader)

add_library(NemesisLoader SHARED
    src/dllmain.cpp
)

set_target_properties(NemesisLoader PROPERTIES
    OUTPUT_NAME "NemesisLoader"
    PREFIX ""
)

if(MSVC)
    # Force /MT (static CRT) for this DLL only, so it has no vcruntime140 dep.
    # We override CMake's default /MD with /NODEFAULTLIB on the dynamic CRT libs
    # plus an explicit /MT compile flag.
    target_compile_options(NemesisLoader PRIVATE
        $<$<CONFIG:Debug>:/MTd>
        $<$<NOT:$<CONFIG:Debug>>:/MT>
    )
    target_link_options(NemesisLoader PRIVATE
        $<$<CONFIG:Debug>:/NODEFAULTLIB:msvcrtd.lib>
        $<$<NOT:$<CONFIG:Debug>>:/NODEFAULTLIB:msvcrt.lib>
    )
endif()
