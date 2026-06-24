cmake_minimum_required(VERSION 3.13)
project(NemesisAll)

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(ASMJIT_STATIC)

add_subdirectory(Infections/src/Nemesis)
add_subdirectory(NemesisLoader/TestDll)
add_subdirectory(NemesisLoader/SecondaryDll)
add_subdirectory(Loader)
