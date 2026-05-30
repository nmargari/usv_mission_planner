#include <raylib.h>
#include "core/config.h"
#include "core/data_model.h"
#include "map/tile_cache.h"
#include "map/map_view.h"
#include "editor/mission_editor.h"

// Simple toolbar button — returns true on click
static bool toolbar_button(float x, float y, float w, float h,
                            const char* label, bool active)
{
    Vector2 mouse   = GetMousePosition();
    bool    hover   = CheckCollisionPointRec(mouse, {x, y, w, h});
    bool    clicked = hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    Color bg = active ? Color{60, 120, 200, 255}
             : hover  ? Color{65,  65,  90, 255}
             :          Color{40,  40,  58, 255};

    DrawRectangleRec({x, y, w, h}, bg);
    DrawRectangleLinesEx({x, y, w, h}, 1.f, Color{90, 90, 120, 255});

    int tw = MeasureText(label, 14);
    DrawText(label,
             static_cast<int>(x + w / 2.f - tw / 2.f),
             static_cast<int>(y + h / 2.f - 7.f),
             14, WHITE);

    return clicked;
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(config::window_w, config::window_h, "USV Mission Planner");
    SetTargetFPS(60);

    mission    current_mission;
    tile_cache cache("assets/tiles/thermaikos");
    map_view   view(GetScreenWidth(), GetScreenHeight(),
                    config::default_lat, config::default_lon,
                    config::default_zoom, cache);

    view.set_top_offset(config::toolbar_h);

    mission_editor editor(current_mission, view);

    while (!WindowShouldClose())
    {
        // ── Update ────────────────────────────────────────────────────────────
        view.update();
        editor.update();

        // ── Draw ──────────────────────────────────────────────────────────────
        BeginDrawing();
        ClearBackground(Color{30, 30, 30, 255});

        view.draw();
        editor.draw();

        // Toolbar background
        DrawRectangle(0, 0, GetScreenWidth(), config::toolbar_h,
                      Color{22, 22, 36, 255});
        DrawLine(0, config::toolbar_h, GetScreenWidth(), config::toolbar_h,
                 Color{60, 60, 85, 255});

        // Waypoint tool toggle
        bool wp_active = editor.current_tool() == editor_tool::place_waypoint;
        if (toolbar_button(8.f, 4.f, 110.f, 32.f, "Add Waypoint", wp_active))
        {
            editor.set_tool(wp_active ? editor_tool::none
                                      : editor_tool::place_waypoint);
        }

        // Tool hint text
        const char* hint = wp_active
            ? "Click on map to place waypoint | Click waypoint to rename"
            : "Click a waypoint to rename it";
        DrawText(hint, 128, 13, 13, Color{160, 160, 180, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
