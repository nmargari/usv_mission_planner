#include "tile_index.h"
#include <cmath>

namespace tile_index
{

static constexpr double k_pi = 3.14159265358979323846;

std::pair<double, double> latlon_to_frac(double lat, double lon, int zoom)
{
    double n  = static_cast<double>(1 << zoom);
    double fx = (lon + 180.0) / 360.0 * n;
    double lr = lat * k_pi / 180.0;
    double fy = (1.0 - std::log(std::tan(lr) + 1.0 / std::cos(lr)) / k_pi) / 2.0 * n;
    return {fx, fy};
}

std::pair<int, int> latlon_to_tile(double lat, double lon, int zoom)
{
    auto [fx, fy] = latlon_to_frac(lat, lon, zoom);
    return {static_cast<int>(fx), static_cast<int>(fy)};
}

std::pair<double, double> tile_to_latlon(int tx, int ty, int zoom)
{
    double n   = static_cast<double>(1 << zoom);
    double lon = static_cast<double>(tx) / n * 360.0 - 180.0;
    double lr  = std::atan(std::sinh(k_pi * (1.0 - 2.0 * ty / n)));
    double lat = lr * 180.0 / k_pi;
    return {lat, lon};
}

} // namespace tile_index
