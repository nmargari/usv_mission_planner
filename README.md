# USV Mission Planner

A desktop Ground Control Station (GCS) for planning missions for an Unmanned Surface Vehicle (USV). The operator places waypoints on an interactive map, draws a geofence, and assembles an ordered sequence of commands using a visual block-based editor. Missions can be saved and loaded as JSON.

---

## Tech Stack

| Component | Version | Role |
|-----------|---------|------|
| **C++17** | — | Language standard |
| **CMake** | ≥ 3.20 | Build system |
| **Raylib** | 4.5 | Window, rendering, input |
| **BehaviorTree.CPP** | 4.9 | Mission execution engine (simulation, upcoming) |
| **nlohmann/json** | 3.11.3 | JSON serialisation (via FetchContent) |

---

## Features

- Interactive map centred on **Thermaikos Bay, Thessaloniki** (pre-downloaded OSM tiles)
- Pan (right-click drag / middle-click drag) and zoom (scroll wheel)
- **Waypoint tool** — left-click to place, click to rename, right-click to delete
- **Geofence tool** — click to add polygon vertices, right-click to close
- **Command panel** — build a mission as an ordered list of blocks:
  - `MOVE_TO_WP` — sail to a named waypoint
  - `SEARCH_AREA` — lawnmower sweep (radius + lane spacing)
  - `LOITER` — hold position for a duration
  - `REPEAT` — repeat the next command N times
  - `RTB` — return to base (always last)
  - Drag blocks to reorder; click a parameter value to edit it inline
- **Export JSON** — saves the full mission to `mission.json`
- **Load JSON** — restores waypoints, geofence, and commands from `mission.json`

---

## Installation

### Prerequisites

Install the following libraries before building:

**Raylib 4.5**
```bash
git clone --branch 4.5 https://github.com/raysan5/raylib.git ~/libs/raylib
cd ~/libs/raylib && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

**BehaviorTree.CPP 4.9**
```bash
git clone --branch 4.9.0 https://github.com/BehaviorTree/BehaviorTree.CPP.git ~/libs/BehaviorTree.CPP
cd ~/libs/BehaviorTree.CPP && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBTCPP_UNIT_TESTS=OFF -DBTCPP_EXAMPLES=OFF
make -j$(nproc)
```

> If you installed the libraries to a different prefix, update the `CMAKE_PREFIX_PATH` entries at the top of `CMakeLists.txt`.

### Download Map Tiles

Run the tile download script from the project root (requires `wget`):

```bash
download_range() {
    local z=$1 x0=$2 x1=$3 y0=$4 y1=$5
    for x in $(seq $x0 $x1); do
        for y in $(seq $y0 $y1); do
            mkdir -p assets/tiles/thermaikos/$z/$x
            local f="assets/tiles/thermaikos/$z/$x/$y.png"
            [ -f "$f" ] && continue
            wget -q --user-agent="USV_Planner/1.0" \
                -O "$f" \
                "https://tile.openstreetmap.org/$z/$x/$y.png"
            sleep 0.1
        done
    done
}

download_range 13  4616  4620  3080  3085
download_range 14  9232  9241  6160  6171
download_range 15 18465 18483 12321 12344
```

Tiles are saved to `assets/tiles/thermaikos/<zoom>/<x>/<y>.png` (~11 MB total).

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
./build/usv_mission_planner
```

The binary looks for `assets/` relative to its location. A symlink is created automatically by CMake.

---

## Usage

### Toolbar buttons

| Button | Action |
|--------|--------|
| **Add Waypoint** | Toggle waypoint placement mode |
| **Geofence** | Toggle geofence drawing mode |
| **Export JSON** | Save mission to `build/mission.json` |
| **Load JSON** | Load mission from `build/mission.json` |

### Map controls

| Input | Action |
|-------|--------|
| Scroll wheel | Zoom in / out |
| Right-click drag | Pan the map |
| Middle-click drag | Pan the map |

### Waypoints

| Input | Action |
|-------|--------|
| Left-click (Add Waypoint mode) | Place a new waypoint; rename box opens |
| Left-click on waypoint | Open rename box |
| Enter | Confirm rename |
| Escape | Cancel rename |
| Right-click on waypoint | Delete waypoint |

### Geofence

| Input | Action |
|-------|--------|
| Left-click (Geofence mode) | Add a polygon vertex |
| Right-click (≥ 3 vertices) | Close and commit the polygon |
| Escape | Cancel in-progress polygon |

### Command panel (right side)

| Action | How |
|--------|-----|
| Add command | Click `MOVE`, `SRCH`, `LOITER`, `RPT`, or `RTB` at the bottom |
| Reorder | Drag the `=` handle on the left of any block |
| Edit parameter | Click the parameter value to edit inline; press Enter to confirm |
| Delete command | Click the `x` button on the right of the block |

---

## Project Structure

```
usv_mission_planner/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                          — window, main loop, toolbar
│   ├── core/
│   │   ├── config.h                      — compile-time constants
│   │   └── data_model.h                  — all shared structs
│   ├── map/
│   │   ├── tile_index.h / .cpp           — Web Mercator tile math
│   │   ├── tile_cache.h / .cpp           — on-demand PNG → Texture2D cache
│   │   └── map_view.h / .cpp             — viewport, pan/zoom, tile rendering
│   ├── editor/
│   │   ├── mission_editor.h / .cpp       — waypoint & geofence tools
│   │   └── command_panel.h / .cpp        — command block list with drag-reorder
│   └── serialization/
│       └── mission_io.h / .cpp           — JSON save / load
└── assets/
    └── tiles/thermaikos/<zoom>/<x>/<y>.png
```

---

## Data Model

```cpp
struct waypoint       { string id; double lat, lon; };
struct geofence       { vector<pair<double,double>> verts; bool enabled; };

enum class cmd_type   { move_to_wp, search_area, loiter, repeat, rtb };

// Per-command parameters (std::variant — no heap allocation)
struct move_to_wp_params  { string wp_id; double arrival_m; };
struct search_area_params { double radius_m, spacing_m; };
struct loiter_params      { double duration_s; };
struct repeat_params      { int count; };
struct rtb_params         {};

struct mission_item   { string id; cmd_type type; cmd_params params; };

struct mission {
    unordered_map<string, waypoint> waypoints;
    geofence                         fence;
    vector<mission_item>             commands;
    string                           name;
};
```

---

## Mission JSON Format

```json
{
  "version": 1,
  "name": "My Mission",
  "waypoints": [
    { "id": "WP_01", "lat": 40.55, "lon": 22.95 }
  ],
  "geofence": {
    "enabled": true,
    "vertices": [
      { "lat": 40.50, "lon": 22.90 },
      { "lat": 40.60, "lon": 22.90 },
      { "lat": 40.60, "lon": 23.00 },
      { "lat": 40.50, "lon": 23.00 }
    ]
  },
  "commands": [
    { "id": "cmd_001", "type": "MOVE_TO_WP",  "params": { "wp_id": "WP_01", "arrival_m": 10.0 } },
    { "id": "cmd_002", "type": "LOITER",       "params": { "duration_s": 30.0 } },
    { "id": "cmd_003", "type": "SEARCH_AREA",  "params": { "radius_m": 200.0, "spacing_m": 40.0 } },
    { "id": "cmd_004", "type": "RTB",          "params": {} }
  ]
}
```

---

## Architecture

```
main.cpp  (window + main loop + toolbar)
│
├── map_view          — single source of truth for all lat/lon ↔ pixel conversions
│   ├── tile_cache    — lazy-loads PNG tiles from disk into GPU textures
│   └── tile_index    — Web Mercator slippy-map math
│
├── mission_editor    — handles map input for waypoint placement and geofence drawing
│   └── command_panel — right-side block list; drag-reorder + inline param editing
│
└── mission_io        — serialises/deserialises the mission struct to/from JSON
```

All subsystems hold a reference to `mission` (owned by `main`). There is no global state. GPU resources (`tile_cache`) are scoped inside the main loop block so they are released before `CloseWindow()` is called.

---

## Coding Style

- **snake_case** for all identifiers (variables, functions, structs, enums, files)
- **Allman braces** — opening `{` always on its own line
- `.h` / `.cpp` file extensions
