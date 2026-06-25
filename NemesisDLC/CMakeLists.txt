cmake_minimum_required(VERSION 3.20)

project(NemesisDLC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "NemesisDLC must be built as x64")
endif()

add_library(NemesisDLC SHARED
    dllmain.cpp
    "Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/CameraPositionChange.cpp"
    "Miscellaneous Utilities/LogsSystem/LogsSystem.cpp"
)

target_include_directories(NemesisDLC PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AllUsedAddresses/Address
    "${CMAKE_CURRENT_SOURCE_DIR}/Miscellaneous Utilities/LogsSystem"
    ${CMAKE_CURRENT_SOURCE_DIR}/RequiredDependencies/minhook-master/include
)

add_subdirectory(RequiredDependencies/minhook-master)
target_link_libraries(NemesisDLC PRIVATE minhook)

set_target_properties(NemesisDLC PROPERTIES
    OUTPUT_NAME "NemesisLoader"
    PREFIX ""
)
