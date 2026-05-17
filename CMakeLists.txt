cmake_minimum_required(VERSION 3.20)
project(WoW_335a_Patcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── TUI (no extra deps) ───────────────────────────────────────────────────────
add_executable(WoW_335a_Patcher src/main.cpp)
if(MINGW)
    target_link_options(WoW_335a_Patcher PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()

# ── GUI (Dear ImGui + SDL2 + OpenGL3) ────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.8          # update to latest tag as needed
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL2)

find_package(OpenGL REQUIRED)

add_executable(WoW_335a_Patcher_GUI
    src/gui_main.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(WoW_335a_Patcher_GUI PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(WoW_335a_Patcher_GUI PRIVATE
    SDL2::SDL2
    OpenGL::GL
)
if(WIN32)
    target_link_libraries(WoW_335a_Patcher_GUI PRIVATE comdlg32)
endif()
if(MINGW)
    target_link_options(WoW_335a_Patcher_GUI PRIVATE
        -static-libgcc -static-libstdc++ -static)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)
