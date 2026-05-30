#include <raylib.h>
#include "core/config.h"
#include "map/tile_cache.h"
#include "map/map_view.h"

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(config::window_w, config::window_h, "USV Mission Planner");
    SetTargetFPS(60);

    tile_cache cache("assets/tiles/thermaikos");
    map_view   view(GetScreenWidth(), GetScreenHeight(),
                    config::default_lat, config::default_lon,
                    config::default_zoom, cache);

    while (!WindowShouldClose())
    {
        view.update();

        BeginDrawing();
        ClearBackground(Color{30, 30, 30, 255});
        view.draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
