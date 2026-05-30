#include "map_view.h"
#include "tile_index.h"
#include "../core/config.h"
#include <raylib.h>
#include <cmath>
#include <algorithm>

static constexpr double k_pi = 3.14159265358979323846;

map_view::map_view(int screen_w, int screen_h,
                   double center_lat, double center_lon,
                   int zoom, tile_cache& cache)
    : center_lat_(center_lat)
    , center_lon_(center_lon)
    , zoom_(zoom)
    , screen_w_(screen_w)
    , screen_h_(screen_h)
    , cache_(cache)
{}

// ── Coordinate conversion ─────────────────────────────────────────────────────

Vector2 map_view::latlon_to_screen(double lat, double lon) const
{
    auto [fx_pt,  fy_pt]  = tile_index::latlon_to_frac(lat,         lon,         zoom_);
    auto [fx_ctr, fy_ctr] = tile_index::latlon_to_frac(center_lat_, center_lon_, zoom_);
    float px = static_cast<float>((fx_pt - fx_ctr) * config::tile_size + screen_w_ / 2.0);
    float py = static_cast<float>((fy_pt - fy_ctr) * config::tile_size + screen_h_ / 2.0);
    return {px, py};
}

void map_view::screen_to_latlon(float px, float py, double& lat, double& lon) const
{
    auto [fx_ctr, fy_ctr] = tile_index::latlon_to_frac(center_lat_, center_lon_, zoom_);
    double fx = fx_ctr + (px - screen_w_ / 2.0) / config::tile_size;
    double fy = fy_ctr + (py - screen_h_ / 2.0) / config::tile_size;
    double n  = static_cast<double>(1 << zoom_);
    lon = fx / n * 360.0 - 180.0;
    lat = std::atan(std::sinh(k_pi * (1.0 - 2.0 * fy / n))) * 180.0 / k_pi;
}

// ── Pan / zoom ────────────────────────────────────────────────────────────────

void map_view::do_pan(float dx, float dy)
{
    auto [fx, fy] = tile_index::latlon_to_frac(center_lat_, center_lon_, zoom_);
    double n  = static_cast<double>(1 << zoom_);
    double nx = std::clamp(fx - dx / config::tile_size, 0.0, n);
    double ny = std::clamp(fy - dy / config::tile_size, 0.0, n);
    center_lon_ = nx / n * 360.0 - 180.0;
    center_lat_ = std::atan(std::sinh(k_pi * (1.0 - 2.0 * ny / n))) * 180.0 / k_pi;
}

void map_view::do_zoom(float wheel, Vector2 mouse)
{
    int new_zoom = std::clamp(zoom_ + (wheel > 0 ? 1 : -1),
                              config::min_zoom, config::max_zoom);
    if (new_zoom == zoom_) return;

    // Remember what geographic point is under the mouse
    double lat_under, lon_under;
    screen_to_latlon(mouse.x, mouse.y, lat_under, lon_under);

    zoom_ = new_zoom;

    // After zoom, re-centre so that lat_under stays under mouse
    auto [fx, fy] = tile_index::latlon_to_frac(lat_under, lon_under, zoom_);
    double n      = static_cast<double>(1 << zoom_);
    double new_fx = fx - (mouse.x - screen_w_ / 2.0) / config::tile_size;
    double new_fy = fy - (mouse.y - screen_h_ / 2.0) / config::tile_size;
    new_fx = std::clamp(new_fx, 0.0, n);
    new_fy = std::clamp(new_fy, 0.0, n);
    center_lon_ = new_fx / n * 360.0 - 180.0;
    center_lat_ = std::atan(std::sinh(k_pi * (1.0 - 2.0 * new_fy / n))) * 180.0 / k_pi;
}

// ── Input ─────────────────────────────────────────────────────────────────────

void map_view::update()
{
    screen_w_ = GetScreenWidth();
    screen_h_ = GetScreenHeight();

    Vector2 mouse   = GetMousePosition();
    bool    in_map  = mouse.y > static_cast<float>(top_offset_);

    // Zoom on scroll wheel (only in map area)
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f && in_map)
        do_zoom(wheel, mouse);

    // Pan on middle-mouse or right-mouse drag (only in map area)
    bool    pan_btn = in_map
                   && (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)
                    || IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

    if (pan_btn)
    {
        if (!panning_)
        {
            panning_   = true;
            pan_prev_x_ = mouse.x;
            pan_prev_y_ = mouse.y;
        }
        else
        {
            do_pan(mouse.x - pan_prev_x_, mouse.y - pan_prev_y_);
            pan_prev_x_ = mouse.x;
            pan_prev_y_ = mouse.y;
        }
    }
    else
    {
        panning_ = false;
    }
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void map_view::draw()
{
    auto [fx_ctr, fy_ctr] = tile_index::latlon_to_frac(center_lat_, center_lon_, zoom_);

    int cx     = static_cast<int>(fx_ctr);
    int cy     = static_cast<int>(fy_ctr);
    int n      = 1 << zoom_;
    int tiles_x = screen_w_  / config::tile_size + 2;
    int tiles_y = screen_h_ / config::tile_size + 2;

    for (int dy = -tiles_y; dy <= tiles_y; ++dy)
    {
        for (int dx = -tiles_x; dx <= tiles_x; ++dx)
        {
            int tx = cx + dx;
            int ty = cy + dy;
            if (tx < 0 || tx >= n || ty < 0 || ty >= n) continue;

            // Pixel position of this tile's top-left corner
            int px = static_cast<int>((tx - fx_ctr) * config::tile_size + screen_w_ / 2.0);
            int py = static_cast<int>((ty - fy_ctr) * config::tile_size + screen_h_ / 2.0);

            DrawTexture(cache_.get(zoom_, tx, ty), px, py, WHITE);
        }
    }

    // HUD: zoom level in bottom-left corner
    DrawText(TextFormat("Zoom %d  |  %.4f, %.4f", zoom_, center_lat_, center_lon_),
             8, screen_h_ - 24, 16, WHITE);
}
