#include <raylib.h>
#include "core/config.h"
#include "core/data_model.h"
#include "map/tile_cache.h"
#include "map/map_view.h"
#include "editor/mission_editor.h"
#include "editor/command_panel.h"

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

    // Scope block: all GPU-owning objects must be destroyed BEFORE CloseWindow(),
    // otherwise UnloadTexture() is called on a dead OpenGL context → segfault.
    {
        mission    current_mission;
        tile_cache cache("assets/tiles/thermaikos");

        map_view view(GetScreenWidth(), GetScreenHeight(),
                      config::default_lat, config::default_lon,
                      config::default_zoom, cache);
        view.set_top_offset(config::toolbar_h);
        view.set_right_offset(config::panel_w);

        mission_editor editor(current_mission, view);

        command_panel panel(current_mission,
                            GetScreenWidth() - config::panel_w,
                            config::toolbar_h,
                            config::panel_w,
                            GetScreenHeight() - config::toolbar_h);

        while (!WindowShouldClose())
        {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            panel.resize(sw - config::panel_w, config::toolbar_h,
                         config::panel_w, sh - config::toolbar_h);

            // ── Update ────────────────────────────────────────────────────────
            view.update();
            editor.update();
            panel.update();

            // ── Draw ──────────────────────────────────────────────────────────
            BeginDrawing();
            ClearBackground(Color{30, 30, 30, 255});

            view.draw();
            editor.draw();
            panel.draw();

            // Toolbar (drawn last so it sits on top)
            DrawRectangle(0, 0, sw, config::toolbar_h, Color{22, 22, 36, 255});
            DrawLine(0, config::toolbar_h, sw, config::toolbar_h, Color{60, 60, 85, 255});

            bool wp_active = editor.current_tool() == editor_tool::place_waypoint;
            if (toolbar_button(8.f, 4.f, 110.f, 32.f, "Add Waypoint", wp_active))
                editor.set_tool(wp_active ? editor_tool::none : editor_tool::place_waypoint);

            const char* hint = wp_active
                ? "Click map to place  |  Click waypoint to rename"
                : "Click a waypoint to rename it";
            DrawText(hint, 128, 13, 13, Color{140, 145, 170, 255});

            EndDrawing();
        }
    } // tile_cache, map_view, etc. destroyed here — OpenGL context still alive

    CloseWindow();
    return 0;
}
