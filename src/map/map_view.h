#pragma once

#include <raylib.h>
#include "tile_cache.h"

// Manages the map viewport: geographic centre, zoom level, pan/zoom input,
// tile rendering, and coordinate conversion between lat/lon and screen pixels.
class map_view
{
public:
    map_view(int screen_w, int screen_h,
             double center_lat, double center_lon,
             int zoom, tile_cache& cache);

    // Call every frame — handles mouse pan and scroll-wheel zoom
    void update();

    // Draw all visible tiles
    void draw();

    // Convert geographic coordinates to screen pixels
    Vector2 latlon_to_screen(double lat, double lon) const;

    // Convert screen pixels to geographic coordinates
    void screen_to_latlon(float px, float py, double& lat, double& lon) const;

    double center_lat() const { return center_lat_; }
    double center_lon() const { return center_lon_; }
    int    zoom_level() const { return zoom_; }

    // Pixels from the top of the window to reserve for the toolbar
    void set_top_offset(int h) { top_offset_ = h; }

private:
    void do_pan(float dx, float dy);
    void do_zoom(float wheel, Vector2 mouse);

    double     center_lat_;
    double     center_lon_;
    int        zoom_;
    int        screen_w_;
    int        screen_h_;
    tile_cache& cache_;

    int   top_offset_  = 0;
    bool  panning_     = false;
    float pan_prev_x_  = 0.f;
    float pan_prev_y_  = 0.f;
};
