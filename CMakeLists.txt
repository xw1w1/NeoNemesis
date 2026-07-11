cmake_minimum_required(VERSION 3.13)
project(Nemesis)

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(ASMJIT_STATIC CONSOLE_TRACE)


add_subdirectory(Injector)
add_subdirectory(NemesisLoader)
add_subdirectory(NemesisClient)
