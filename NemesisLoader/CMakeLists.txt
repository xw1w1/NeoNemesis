cmake_minimum_required(VERSION 3.13)
project(NemesisLoader)

if(MSVC)
    enable_language(RC)
endif()

add_library(NemesisLoader SHARED
    src/dllmain.cpp
    src/config/Config.cpp
    src/audio/KillSound.cpp
    src/kill/KillWatcher.cpp
    src/hook/SoundHook.cpp
    src/audio/resources.rc

    src/address/AllUsedAddresses/AllUsedAddresses.hpp
    src/config/Config.hpp
    src/audio/KillSound.hpp
    src/audio/resource.h
    src/kill/KillWatcher.hpp
    src/hook/SoundHook.hpp
)

target_include_directories(NemesisLoader PRIVATE src)
target_link_libraries(NemesisLoader PRIVATE winmm shell32)

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
