#pragma once

#include <utility>

// Slippy-map (OSM) tile coordinate math.
// All functions use the standard Web Mercator projection (EPSG:3857).
namespace tile_index
{
    // Fractional tile position {fx, fy} for a lat/lon at the given zoom.
    // Fractional means the integer part is the tile column/row and the
    // decimal part is the offset within that tile.
    std::pair<double, double> latlon_to_frac(double lat, double lon, int zoom);

    // Integer tile column and row for a lat/lon at the given zoom.
    std::pair<int, int> latlon_to_tile(double lat, double lon, int zoom);

    // Top-left corner lat/lon of the tile at (tx, ty, zoom).
    std::pair<double, double> tile_to_latlon(int tx, int ty, int zoom);
}
