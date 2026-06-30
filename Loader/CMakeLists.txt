cmake_minimum_required(VERSION 3.13)
project(Loader)

set(NEM_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/../NemesisDLC/RequiredDependencies")

add_executable(Loader WIN32
    main.cpp 
    monitor.cpp
    "${NEM_DEPS}/imgui-master/imgui.cpp"
    "${NEM_DEPS}/imgui-master/imgui_draw.cpp"
    "${NEM_DEPS}/imgui-master/imgui_widgets.cpp"
    "${NEM_DEPS}/imgui-master/imgui_tables.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_win32.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_dx11.cpp"
  "product.cpp")

set_target_properties(Loader PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")

target_include_directories(Loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/Nemesis
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/3rd_party
    "${NEM_DEPS}/imgui-master"
    "${NEM_DEPS}/imgui-master/backends"
)

target_link_libraries(Loader PRIVATE 
    Nemesis
    d3d11
    dxgi
    d3dcompiler
    ${CMAKE_CURRENT_SOURCE_DIR}/../Infections/src/3rd_party/DIA/lib/amd64/diaguids.lib
)
