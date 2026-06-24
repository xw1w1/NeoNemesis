cmake_minimum_required(VERSION 3.13)
project(Loader)

add_executable(Loader main.cpp)
target_include_directories(Loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/Nemesis
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/3rd_party
)
target_link_libraries(Loader PRIVATE Nemesis
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/3rd_party/DIA/lib/amd64/diaguids.lib
)
