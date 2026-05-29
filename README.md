# USV Mission Planner — Full Project Plan

## Coding Style
- **snake_case** for all identifiers (variables, functions, structs, enums, filenames)
- **Allman braces** — opening `{` always on its own line
- `.h` / `.cpp` file extensions throughout

---

## Environment
- C++17, CMake ≥ 3.20
- Raylib 4.5 installed at `/home/forgo/libs/raylib` (cmake config: `cmake/raylib-config.cmake`)
- BehaviorTree.CPP 4.9 installed at `/home/forgo/libs/BehaviorTree.CPP` (cmake config: `build/`)
- nlohmann/json via FetchContent (header-only)
- Pre-downloaded OSM tiles for Thermaikos Bay at `assets/tiles/thermaikos/<zoom>/<x>/<y>.png`

---

## 1. Folder / File Structure

```
usv_mission_planner/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── config.h              — compile-time constants (tile size, default zoom, speeds, layout)
│   │   ├── data_model.h          — all shared structs: waypoint, geofence, mission_item, mission, sim_state
│   │   ├── geo_math.h / .cpp     — haversine distance, bearing, move-along-bearing, point-in-polygon
│   │   └── lawnmower.h / .cpp    — sweep-line pattern generator (geofence → ordered lat/lon waypoints)
│   ├── map/
│   │   ├── tile_index.h / .cpp   — slippy-map math: lat/lon ↔ tile x/y/zoom, fractional tile position
│   │   ├── tile_cache.h / .cpp   — loads PNG tiles from disk into Raylib Texture2D, keyed by {zoom,x,y}
│   │   └── map_view.h / .cpp     — viewport state (center lat/lon, zoom), pan/zoom, lat/lon↔pixel, tile draw
│   ├── editor/
│   │   ├── mission_editor.h / .cpp  — EDITOR state coordinator: map input, tool dispatch, pan/zoom
│   │   └── command_panel.h / .cpp   — right-panel command block list with drag-reorder and add/remove
│   ├── validator/
│   │   └── validator.h / .cpp    — validate(mission) → validation_result with list of errors
│   ├── simulator/
│   │   ├── sim_engine.h / .cpp   — BT tree lifecycle (init/tick/stop), blackboard, geofence guard
│   │   └── sim_renderer.h / .cpp — draws USV icon, trail, lawnmower animation, violation flash
│   ├── bt_nodes/
│   │   ├── move_to_wp_node.h / .cpp
│   │   ├── search_area_node.h / .cpp
│   │   ├── loiter_node.h / .cpp
│   │   ├── rtb_node.h / .cpp
│   │   └── register_nodes.h      — single call: register_all_nodes(BT::BehaviorTreeFactory&)
│   ├── serialization/
│   │   └── mission_io.h / .cpp   — save(mission, path), load(path) → mission, JSON schema v1
│   └── ui/
│       ├── app_controller.h / .cpp  — owns AppState enum, main loop, wires all subsystems together
│       ├── top_bar.h / .cpp         — toolbar: file ops, tool toggles, Run/Pause/Stop, speed slider
│       └── widgets.h / .cpp         — reusable Raylib-primitive widgets: button, slider, scroll_panel
├── assets/
│   └── tiles/
│       └── thermaikos/
│           └── <zoom>/<x>/<y>.png
└── scripts/
    └── download_tiles.sh         — downloads Thermaikos Bay OSM tiles via wget/curl
```

---

## 2. Core Data Models (`src/core/data_model.h`)

```cpp
struct waypoint
{
    std::string id;           // "WP_01"
    double      lat, lon;     // WGS84 decimal degrees
    mutable float px, py;     // screen pixel cache; invalidated on pan/zoom
};

struct geofence
{
    std::vector<std::pair<double,double>> verts;  // (lat,lon) ordered, closed polygon
    bool enabled = true;
};

enum class cmd_type { move_to_wp, search_area, loiter, repeat, rtb };

struct move_to_wp_params  { std::string wp_id;  double arrival_m = 10.0; };
struct search_area_params { double radius_m = 200.0; double spacing_m = 40.0; };
struct loiter_params      { double duration_s = 30.0; };
struct repeat_params      { int count = 1; };   // repeats the NEXT command N times
struct rtb_params         {};

using cmd_params = std::variant<
    move_to_wp_params, search_area_params, loiter_params, repeat_params, rtb_params>;

struct mission_item
{
    std::string id;       // "cmd_001"
    cmd_type    type;
    cmd_params  params;
};

struct mission
{
    std::unordered_map<std::string, waypoint> waypoints;
    geofence                                   fence;
    std::vector<mission_item>                  commands;
    std::string                                home_wp_id;
    std::string                                name;
};

enum class sim_status { running, paused, completed, violation, error };

struct sim_state
{
    double     lat = 0, lon = 0;
    float      heading = 0;       // degrees CW from North
    double     speed_mps = 0;
    int        active_cmd = -1;
    sim_status status = sim_status::running;
    double     elapsed_s = 0;
    std::vector<std::pair<double,double>> trail;       // recent positions (wake)
    std::vector<std::pair<double,double>> sweep_done;  // visited lawnmower points
};
```

**Key decisions:**
- `cmd_params` is a `std::variant` — no heap allocation per command, trivially serialisable via `std::visit`
- `repeat_params` has a single `count`; it always wraps exactly the next `mission_item` — maps directly to `BT::RepeatNode` with one child
- `waypoint::px/py` is a mutable cache updated by `map_view` on each draw call; never serialised

---

## 3. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(usv_mission_planner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ── Pre-installed library paths ───────────────────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH
    "/home/forgo/libs/raylib"
    "/home/forgo/libs/BehaviorTree.CPP/build"
)

find_package(raylib           REQUIRED)
find_package(behaviortree_cpp REQUIRED)

# nlohmann/json — header-only, fetched once and cached
include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ── Main executable ───────────────────────────────────────────────────────────
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(usv_mission_planner ${APP_SOURCES})

target_include_directories(usv_mission_planner PRIVATE src/)

target_link_libraries(usv_mission_planner PRIVATE
    raylib
    behaviortree_cpp
    nlohmann_json::nlohmann_json
)

# Assets symlink so the binary finds tiles at runtime
add_custom_command(TARGET usv_mission_planner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:usv_mission_planner>/assets
    COMMENT "Symlinking assets/")
```

---

## 4. BehaviorTree.CPP Node Design

### Blackboard layout (string constants in `sim_engine.h`)

| Key | C++ type | Written by | Read by |
|-----|----------|-----------|---------|
| `"mission"` | `const mission*` | sim_engine | all nodes |
| `"sim_state"` | `sim_state*` | sim_engine | all nodes |
| `"dt"` | `double` | sim_engine (each tick) | move_to_wp, loiter, rtb, search_area |
| `"speed_mps"` | `double` | sim_engine (init) | move_to_wp, rtb, search_area |
| `"home_lat"`, `"home_lon"` | `double` | sim_engine (init) | rtb_node |
| `"sweep_pts"` | `std::vector<…>*` | search_area_node::on_start | search_area_node |
| `"sweep_idx"` | `int` | search_area_node | search_area_node |

### Node types

| Command | BT node class | Base class |
|---------|--------------|------------|
| `move_to_wp` | `move_to_wp_node` | `BT::StatefulActionNode` |
| `search_area` | `search_area_node` | `BT::StatefulActionNode` |
| `loiter` | `loiter_node` | `BT::StatefulActionNode` |
| `repeat` | built-in `BT::RepeatNode` | — |
| `rtb` | `rtb_node` | `BT::StatefulActionNode` |

**Movement pattern** (shared by `move_to_wp`, `search_area`, `rtb`):
Each `on_running()` call computes bearing to target, advances USV by `speed * dt` via `geo_math::move()`, writes new position to `sim_state`. Returns `SUCCESS` when `distance <= arrival_m`.

**`search_area_node::on_start()`** calls `lawnmower::generate(fence, radius_m, spacing_m)` to produce the sweep waypoint list, stores it on the blackboard, then drives through each point in sequence during `on_running()`.

### Tree XML (built dynamically by `sim_engine::build_xml()`)

```xml
<root BTCPP_format="4">
  <BehaviorTree ID="mission">
    <Sequence>
      <move_to_wp wp_id="WP_01" arrival_m="10"/>
      <Repeat num_cycles="3">
        <loiter duration_s="20"/>
      </Repeat>
      <search_area radius_m="200" spacing_m="40"/>
      <rtb/>
    </Sequence>
  </BehaviorTree>
</root>
```

`build_xml()` iterates `mission.commands`; on a `repeat` item it emits `<Repeat num_cycles="N">` wrapping the next item, then skips that next item in the outer loop.

---

## 5. Application State Machine

```
EDITOR ──[Run clicked]──► VALIDATING ──[errors]──► EDITOR (show error modal)
                               │
                           [valid]
                               ▼
                          SIMULATING ◄──[Resume]── PAUSED
                               │                      ▲
                           [Pause]────────────────────┘
                               │
                     [tree SUCCESS/FAILURE]
                               ▼
                           RESULTS ──[Back]──► EDITOR
```

**`app_controller`** owns the `app_state` enum and each frame calls into the active subsystems:

| State | What runs | TopBar shows |
|-------|-----------|-------------|
| `editor` | `mission_editor::update/draw`, `command_panel::draw` | New, Save, Load, WP tool, GF tool, Run |
| `validating` | `validator::validate()` — synchronous, completes in one frame | — |
| `simulating` | `sim_engine::tick(dt * speed)`, `sim_renderer::draw` | Pause, Stop, speed slider |
| `paused` | `sim_renderer::draw` only (frozen) | Resume, Stop |
| `results` | `sim_renderer::draw` + results overlay | Back to Editor |

---

## 6. Validation Rules

All errors are collected (not fail-fast) so the operator sees everything at once:

1. Mission has at least one command
2. RTB is the last command (and exists)
3. No REPEAT as the last command or immediately before RTB
4. REPEAT does not exceed remaining commands before RTB
5. Every `move_to_wp` references an existing waypoint id
6. `home_wp_id` exists in `mission.waypoints`
7. Geofence has ≥ 3 vertices if any `search_area` command is present
8. Each `search_area` circle is fully inside the geofence

---

## 7. Recommended Build Order

| # | Milestone | Goal |
|---|-----------|------|
| 1 | CMake scaffold | `CMakeLists.txt` + minimal `main.cpp` — confirms all three deps link |
| 2 | Geo + tile math | `geo_math`, `lawnmower`, `tile_index` |
| 3 | Map renders | `tile_cache`, `map_view` with pan/zoom — tiles visible on screen |
| 4 | Data + serialiser | `data_model.h`, `mission_io` — save/load a hardcoded mission to JSON |
| 5 | Editor tools | `mission_editor` (waypoint click, geofence draw), map overlays |
| 6 | Command panel + UI | `command_panel`, `widgets`, `top_bar`, `app_controller` state skeleton |
| 7 | Validator | `validator` wired into Run button; error modal on failure |
| 8 | BT nodes + SimEngine | All 4 custom nodes + `sim_engine::init/tick/stop` |
| 9 | SimRenderer | USV icon, trail, lawnmower animation, geofence violation flash |
| 10 | Results + polish | Results overlay, keyboard shortcuts, mission name edit |

---

## 8. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        app_controller                           │
│  owns: mission, app_state, all subsystem instances              │
│  drives: main loop → update → draw → handle events             │
└────────┬───────────────────────────────────────┬────────────────┘
         │ EDITOR mode                           │ SIMULATING mode
         ▼                                       ▼
┌─────────────────────┐               ┌──────────────────────┐
│   mission_editor    │               │      sim_engine       │
│  - pan/zoom input   │               │  - builds BT tree XML │
│  - waypoint tool    │               │  - ticks BT::Tree     │
│  - geofence tool    │               │  - owns blackboard    │
└────────┬────────────┘               └──────────┬───────────┘
         │                                        │ reads/writes
         ▼                                        ▼
┌─────────────────────┐               ┌──────────────────────┐
│   command_panel     │               │      bt_nodes/        │
│  - block list UI    │               │  move_to_wp_node      │
│  - drag-reorder     │               │  search_area_node     │
│  - add/remove cmds  │               │  loiter_node          │
└─────────────────────┘               │  rtb_node             │
                                       └──────────┬───────────┘
                  ┌──────────────────────────────►│ write sim_state
                  │                               ▼
         ┌────────────────┐           ┌──────────────────────┐
         │    map_view    │◄──────────│    sim_renderer       │
         │  - tile draw   │  latlon   │  - USV icon + trail   │
         │  - pan/zoom    │  to pixel │  - sweep animation    │
         │  - overlays    │           │  - violation flash    │
         └────────┬───────┘           └──────────────────────┘
                  │
         ┌────────▼───────┐
         │   tile_cache   │
         │ {zoom,x,y}→Tex │
         └────────────────┘
```

**Data flows top-to-bottom:** `app_controller` holds the `mission` struct and passes references down. No module owns more than its own subsystem. `map_view` is the single source of truth for all coordinate conversions.
