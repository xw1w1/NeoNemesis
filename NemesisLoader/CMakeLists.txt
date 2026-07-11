cmake_minimum_required(VERSION 3.13)
project(NemesisLoader)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Loader must be built as x64")
endif()

set(NEM_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/../NemesisClient/RequiredDependencies")

add_executable(NemesisLoader WIN32
    main.cpp 
    monitor.cpp
    "${NEM_DEPS}/imgui-master/imgui.cpp"
    "${NEM_DEPS}/imgui-master/imgui_draw.cpp"
    "${NEM_DEPS}/imgui-master/imgui_widgets.cpp"
    "${NEM_DEPS}/imgui-master/imgui_tables.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_win32.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_dx11.cpp"
    "subscription/product.cpp" 
    "imgui_extensions.cpp" 
    "layout_utils.cpp" 
    "process_helper.h" 
    "file_utils.cpp"
    "steam/steamacc.cpp"
    "steam/steam_helper.cpp"
    "steam/avatar_loader.cpp"
    "system_info.cpp"
    "product_registry.cpp"
    "api/loader_api.cpp"
    "api/json.h"
    "subscription/remote_profile.cpp" 
    "injector/injector.cpp" 
    "injector/infector.h"
 "neo/LoaderUI.cpp")

set_target_properties(NemesisLoader PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

target_include_directories(NemesisLoader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/Nemesis
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/3rd_party
    "${NEM_DEPS}/imgui-master"
    "${NEM_DEPS}/imgui-master/backends"
)

target_link_libraries(NemesisLoader PRIVATE 
    InjectorLib
    d3d11
    dxgi
    d3dcompiler
    ${CMAKE_CURRENT_SOURCE_DIR}/../Injector/src/3rd_party/DIA/lib/amd64/diaguids.lib
)
