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

# BT.CPP — manually declare the imported target (library was built but not installed)
add_library(BT::behaviortree_cpp SHARED IMPORTED GLOBAL)
set_target_properties(BT::behaviortree_cpp PROPERTIES
    IMPORTED_LOCATION "/home/forgo/libs/BehaviorTree.CPP/build/libbehaviortree_cpp.so"
    INTERFACE_INCLUDE_DIRECTORIES "/home/forgo/libs/BehaviorTree.CPP/include"
)

# nlohmann/json — header-only
include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/map/tile_index.cpp
    src/map/tile_cache.cpp
    src/map/map_view.cpp
    src/editor/mission_editor.cpp
    src/editor/command_panel.cpp
    src/serialization/mission_io.cpp
    src/simulator/sim_engine.cpp
)


add_executable(usv_mission_planner ${SOURCES})

target_include_directories(usv_mission_planner PRIVATE src/)

target_link_libraries(usv_mission_planner PRIVATE
    raylib
    BT::behaviortree_cpp
    nlohmann_json::nlohmann_json
)

add_custom_command(TARGET usv_mission_planner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:usv_mission_planner>/assets
    COMMENT "Symlinking assets/")
