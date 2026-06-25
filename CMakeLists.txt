cmake_minimum_required(VERSION 3.13)
project(NemesisAll)

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(ASMJIT_STATIC CONSOLE_TRACE)

add_subdirectory(Infections/src/Nemesis)
add_subdirectory(NemesisLoader)
add_subdirectory(Loader)
