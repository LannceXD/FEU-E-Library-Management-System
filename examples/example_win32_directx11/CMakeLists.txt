cmake_minimum_required(VERSION 3.10)
project(example_win32_directx11 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build settings
add_definitions(-DUNICODE -D_UNICODE)

# Source files for ImGui + backends used by this example
set(IMGUI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../backends/imgui_impl_dx11.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../backends/imgui_impl_win32.cpp"
)

set(EXTRAS
    "${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h"
)

add_executable(example_win32_directx11 main.cpp ${IMGUI_SOURCES} ${EXTRAS})

target_include_directories(example_win32_directx11 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../backends"
)

if (MSVC)
    target_compile_options(example_win32_directx11 PRIVATE /utf-8)
endif()

# Link DirectX libraries commonly required
find_library(D3D11_LIB d3d11 HINTS ENV "DXSDK_DIR" PATHS "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_LIBRARY_PATH}" NO_DEFAULT_PATH)
# Fall back to system library names if find_library didn't locate
if (NOT D3D11_LIB)
    target_link_libraries(example_win32_directx11 PRIVATE d3d11)
else()
    target_link_libraries(example_win32_directx11 PRIVATE ${D3D11_LIB})
endif()

# d3dcompiler
find_library(D3DCOMPILER_LIB d3dcompiler HINTS ENV "DXSDK_DIR" PATHS "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_LIBRARY_PATH}" NO_DEFAULT_PATH)
if (NOT D3DCOMPILER_LIB)
    target_link_libraries(example_win32_directx11 PRIVATE d3dcompiler)
else()
    target_link_libraries(example_win32_directx11 PRIVATE ${D3DCOMPILER_LIB})
endif()

# Helpful message
message(STATUS "Configured example_win32_directx11 target")
message(STATUS "Source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
