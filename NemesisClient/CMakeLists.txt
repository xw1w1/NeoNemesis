cmake_minimum_required(VERSION 3.20)

project(NemesisClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "NemesisClient must be built as x64")
endif()

set(NEM_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/RequiredDependencies")

set(NEM_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
set(NEM_RES_GEN "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_resources.cpp")

file(GLOB NEM_RES_FILES CONFIGURE_DEPENDS "${NEM_RES_DIR}/*")

#file(GLOB NEM_EMBEDDED_MODELS CONFIGURE_DEPENDS
#    "${CMAKE_CURRENT_SOURCE_DIR}/CUSTOM RENDER/CustomModel/generated/*.cpp")

add_custom_command(
    OUTPUT "${NEM_RES_GEN}"
    COMMAND "${CMAKE_COMMAND}"
        -DRES_DIR=${NEM_RES_DIR}
        -DOUT_CPP=${NEM_RES_GEN}
        -P "embed_resources.cmake"
    DEPENDS ${NEM_RES_FILES} "embed_resources.cmake"
    COMMENT "Embedding ../Resources/ into DLL..."
    VERBATIM
)

set(NEM_IMGUI_SOURCES
    "${NEM_DEPS}/imgui-master/imgui.cpp"
    "${NEM_DEPS}/imgui-master/imgui_draw.cpp"
    "${NEM_DEPS}/imgui-master/imgui_tables.cpp"
    "${NEM_DEPS}/imgui-master/imgui_widgets.cpp"
    "${NEM_DEPS}/imgui-master/imgui_demo.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_dx11.cpp"
    "${NEM_DEPS}/imgui-master/backends/imgui_impl_win32.cpp"
)

add_library(NemesisClient SHARED
    dllmain.cpp
    "${NEM_RES_GEN}"
    "Functions/Vision/CameraPositionChange/CameraPositionChange.cpp"
    "Functions/Vision/CustomSkinsgun/CustomSkins.cpp"
    "Functions/Vision/InventoryGive/InventoryGive.cpp"
    "Misc/LogsSystem.cpp"
    "Misc/SvCheats.cpp"
    "Functions/R Functions/JumpSystem/JumpSystem.cpp"
    "RenderHook.cpp"
#    "InactiveFileStorage/testicon/TestIcon.cpp"
#    "CUSTOM RENDER/CustomModel/CustomModel.cpp"
#    "ConfigSavingSystem/FriendModels.cpp"
#    "ConfigSavingSystem/AdminConsole.cpp"
#    ${NEM_EMBEDDED_MODELS}
    "Functions/L Functions/Esp/Esp.cpp"
    "Functions/L Functions/EnemyChams/EnemyChams.cpp"
    "Functions/L Functions/LegitBot/LegitBot.cpp"
#    "Functions/L Functions/WallHackV2/WallHackV2.cpp"
#    "Functions/L Functions/WallHackV2/ModelChams.cpp"
    "Misc/Visibility.cpp"
    "Functions/R Functions/NemsisProject/RageBot.cpp"
    "Functions/R Functions/NemsisProject/SilentAim.cpp"
    "PacketGuard.cpp"
#    "CustomWorldRender/Night/Night.cpp"
#    "System API Rendering/SystemShaderBloom/SystemShaderBloomImpl.cpp"
    "${NEM_DEPS}/kiero2-master/kiero_intern.cpp"
    "${NEM_DEPS}/kiero2-master/kiero_d3d11.cpp"
    ${NEM_IMGUI_SOURCES}
    "resource_loader.cpp"
    "ImGuiUI/imgui_extensions.cpp"
    "ImGuiUI/layout_utils.cpp"
    "ImGuiUI/nemesis_ui.cpp"
)

target_compile_options(NemesisClient PRIVATE /arch:AVX)

target_include_directories(NemesisClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Misc"
 #   "${CMAKE_CURRENT_SOURCE_DIR}/System API Rendering/SystemShaderBloom"
    "${NEM_DEPS}/asmjit-master"
    "${NEM_DEPS}/Detours-main/src"
    "${NEM_DEPS}/DirectXShaderCompiler-main/include"
    "${NEM_DEPS}/DirectXMath-main/Inc"
    "${NEM_DEPS}/ensia-main"
    "${NEM_DEPS}/imgui-master"
    "${NEM_DEPS}/imgui-master/backends"
    "${NEM_DEPS}/kiero2-master"
    "${NEM_DEPS}/mimalloc-main/include"
    "${NEM_DEPS}/minhook-master/include"
)

add_subdirectory(RequiredDependencies/minhook-master)

set(ASMJIT_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(RequiredDependencies/asmjit-master)

set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
add_subdirectory(RequiredDependencies/mimalloc-main)

target_link_libraries(NemesisClient PRIVATE
    minhook
    asmjit
    mimalloc-static
    Psapi
    d3d11
    dxgi
    d3dcompiler
)

set_target_properties(NemesisClient PROPERTIES
    OUTPUT_NAME "NemesisClient"
    PREFIX ""
)
