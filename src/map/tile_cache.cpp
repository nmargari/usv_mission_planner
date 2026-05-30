#include "tile_cache.h"
#include <raylib.h>
#include <string>
#include <filesystem>

tile_cache::tile_cache(const std::string& tiles_root)
    : tiles_root_(tiles_root)
{
    // Grey placeholder for missing tiles — must be created after InitWindow()
    Image img = GenImageColor(256, 256, Color{100, 100, 100, 255});
    placeholder_ = LoadTextureFromImage(img);
    UnloadImage(img);
}

tile_cache::~tile_cache()
{
    for (auto& [key, tex] : textures_)
        UnloadTexture(tex);
    UnloadTexture(placeholder_);
}

const Texture2D& tile_cache::get(int zoom, int x, int y)
{
    tile_key key{zoom, x, y};

    auto it = textures_.find(key);
    if (it != textures_.end())
        return it->second;

    std::string path = tiles_root_
        + "/" + std::to_string(zoom)
        + "/" + std::to_string(x)
        + "/" + std::to_string(y) + ".png";

    if (std::filesystem::exists(path))
    {
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id != 0)
        {
            auto [ins, _] = textures_.emplace(key, tex);
            return ins->second;
        }
    }

    return placeholder_;
}
