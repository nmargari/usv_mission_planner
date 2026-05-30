cmake_minimum_required(VERSION 3.20)
project(usv_mission_planner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ── Pre-installed libraries ───────────────────────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH
    "/home/forgo/libs/raylib"
    "/home/forgo/libs/raylib/cmake"
    "/home/forgo/libs/BehaviorTree.CPP/build"
)

find_package(raylib REQUIRED)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/map/tile_index.cpp
    src/map/tile_cache.cpp
    src/map/map_view.cpp
    src/editor/mission_editor.cpp
    src/editor/command_panel.cpp
)


add_executable(usv_mission_planner ${SOURCES})

target_include_directories(usv_mission_planner PRIVATE src/)

target_link_libraries(usv_mission_planner PRIVATE raylib)

add_custom_command(TARGET usv_mission_planner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:usv_mission_planner>/assets
    COMMENT "Symlinking assets/")
