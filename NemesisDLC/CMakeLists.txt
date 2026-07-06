cmake_minimum_required(VERSION 3.20)

project(NemesisDLC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "NemesisDLC must be built as x64")
endif()

set(NEM_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/RequiredDependencies")

set(NEM_IMGUI_SOURCES
    "${NEM_DEPS}/imgui-master/imgui.cpp"
    "${NEM_DEPS}/imgui-master/imgui_draw.cpp"
    "${NEM_DEPS}/imgui-master/imgui_tables.cpp"
    "${NEM_DEPS}/imgui-master/imgui_widgets.cpp"
    "${NEM_DEPS}/imgui-master/imgui_demo.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_dx11.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_win32.cpp"
    "${NEM_DEPS}/implot-master/implot.cpp"
    "${NEM_DEPS}/implot-master/implot_items.cpp"
    "${NEM_DEPS}/ImGuiColorTextEdit-master/TextEditor.cpp"
    "${NEM_DEPS}/HImGuiAnimation-main/HImGuiAnimation.cpp"
)

add_library(NemesisDLC SHARED
    dllmain.cpp
    "resources.rc"
    "Miscellaneous Functions/UnusualNewVisions/CameraPositionChange/CameraPositionChange.cpp"
    "Miscellaneous Functions/UnusualNewVisions/CustomSkinsgun/CustomSkins.cpp"
    "Miscellaneous Functions/UnusualNewVisions/InventoryGive/InventoryGive.cpp"
    "Miscellaneous Utilities/LogsSystem/LogsSystem.cpp"
    "Miscellaneous Utilities/sc_cheat/SvCheats.cpp"
    "Miscellaneous Functions/R Functions/JumpSystem/JumpSystem.cpp"
    "RenderDrx11/RenderHook.cpp"
    "Miscellaneous Functions/L functions/Esp/Esp.cpp"
    "Miscellaneous Functions/L functions/LegitBot/LegitBot.cpp"
    "Miscellaneous Utilities/Visibility Check System/Visibility.cpp"
    "Miscellaneous Functions/R Functions/NemsisProject/RageBot.cpp"
    "Miscellaneous Functions/R Functions/NemsisProject/SilentAim.cpp"
    "Server Packet Anomaly Guard/Search for Anomalies in Packages/PacketGuard.cpp"
    "System API Rendering/SystemShaderBloom/SystemShaderBloomImpl.cpp"
    "${NEM_DEPS}/kiero2-master/kiero_intern.cpp"
    "${NEM_DEPS}/kiero2-master/kiero_d3d11.cpp"
    ${NEM_IMGUI_SOURCES}
    "resource_loader.cpp"
    "ImGuiUI/imgui_extensions.cpp"
    "ImGuiUI/layout_utils.cpp"
    "ImGuiUI/nemesis_ui.cpp"
)

target_compile_options(NemesisDLC PRIVATE /arch:AVX)

target_include_directories(NemesisDLC PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AllUsedAddresses/Address
    "${CMAKE_CURRENT_SOURCE_DIR}/Miscellaneous Utilities/LogsSystem"
    "${CMAKE_CURRENT_SOURCE_DIR}/System API Rendering/SystemShaderBloom"
    "${NEM_DEPS}/asmjit-master"
    "${NEM_DEPS}/Detours-main/src"
    "${NEM_DEPS}/DirectXShaderCompiler-main/include"
    "${NEM_DEPS}/DirectXMath-main/Inc"
    "${NEM_DEPS}/google Fronst"
    "${NEM_DEPS}/ensia-main"
    "${NEM_DEPS}/HImGuiAnimation-main"
    "${NEM_DEPS}/IconFontCppHeaders-main"
    "${NEM_DEPS}/imgui-master"
    "${NEM_DEPS}/imgui-master/backends"
    "${NEM_DEPS}/ImGuiColorTextEdit-master"
    "${NEM_DEPS}/implot-master"
    "${NEM_DEPS}/kiero2-master"
    "${NEM_DEPS}/mimalloc-main/include"
    "${NEM_DEPS}/minhook-master/include"
    "${NEM_DEPS}/omni-main/single_header"
    "${NEM_DEPS}/omni-main/include"
    "${NEM_DEPS}/PolyHook_2_0-master"
    "${NEM_DEPS}/RetSpoofer-master"
    "${NEM_DEPS}/zydis-master/include"
    "${NEM_DEPS}/zydis-master/dependencies/zycore/include"
)

add_subdirectory(RequiredDependencies/minhook-master)

set(ASMJIT_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(RequiredDependencies/asmjit-master)

set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
add_subdirectory(RequiredDependencies/mimalloc-main)

target_link_libraries(NemesisDLC PRIVATE
    minhook
    asmjit
    mimalloc-static
    Psapi
    d3d11
    dxgi
    d3dcompiler
)

set_target_properties(NemesisDLC PROPERTIES
    OUTPUT_NAME "NemesisLoader"
    PREFIX ""
)
