cmake_minimum_required(VERSION 3.20)
project(WoW_335a_Patcher)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(WoW_335a_Patcher WIN32 src/gui_main.cpp)

target_compile_options(WoW_335a_Patcher PRIVATE
    -Os                          # optimise for size
    -flto                        # link-time optimisation
    -ffunction-sections          # one section per function → gc-sections prunes dead code
    -fdata-sections
    -fno-exceptions              # no exception handling tables
    -fno-rtti                    # no typeinfo / dynamic_cast tables
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fomit-frame-pointer
    -march=x86-64                # baseline; change to native if you don't need XP compat
)

target_link_libraries(WoW_335a_Patcher PRIVATE comctl32 comdlg32 shell32)

target_link_options(WoW_335a_Patcher PRIVATE
    -static-libgcc -static-libstdc++ -static
    -flto
    -Wl,--gc-sections            # discard unreferenced sections
    -Wl,-s                       # strip at link time (no separate strip step needed)
    -Wl,--build-id=none          # drop build-id note
)

set(CMAKE_VERBOSE_MAKEFILE ON)
