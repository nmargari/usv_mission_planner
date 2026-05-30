#pragma once

#include <raylib.h>
#include <unordered_map>
#include <string>

struct tile_key
{
    int zoom, x, y;

    bool operator==(const tile_key& o) const
    {
        return zoom == o.zoom && x == o.x && y == o.y;
    }
};

struct tile_key_hash
{
    std::size_t operator()(const tile_key& k) const noexcept
    {
        std::size_t h = static_cast<std::size_t>(k.zoom);
        h ^= std::hash<int>{}(k.x) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

// Loads PNG tiles from disk on demand and caches them as Raylib Texture2D.
// Tiles are loaded lazily — only when first requested.
// Returns a grey placeholder for any tile not found on disk.
class tile_cache
{
public:
    explicit tile_cache(const std::string& tiles_root);
    ~tile_cache();

    // Non-copyable — owns GPU textures
    tile_cache(const tile_cache&)            = delete;
    tile_cache& operator=(const tile_cache&) = delete;

    const Texture2D& get(int zoom, int x, int y);

private:
    std::string tiles_root_;
    std::unordered_map<tile_key, Texture2D, tile_key_hash> textures_;
    Texture2D placeholder_;
};
