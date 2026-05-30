#include "mission_io.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <variant>

using json = nlohmann::json;

// ── cmd_type ↔ string ─────────────────────────────────────────────────────────

static const char* type_to_str(cmd_type t)
{
    switch (t)
    {
        case cmd_type::move_to_wp:  return "MOVE_TO_WP";
        case cmd_type::search_area: return "SEARCH_AREA";
        case cmd_type::loiter:      return "LOITER";
        case cmd_type::repeat:      return "REPEAT";
        case cmd_type::rtb:         return "RTB";
    }
    return "UNKNOWN";
}

static cmd_type str_to_type(const std::string& s)
{
    if (s == "MOVE_TO_WP")  return cmd_type::move_to_wp;
    if (s == "SEARCH_AREA") return cmd_type::search_area;
    if (s == "LOITER")      return cmd_type::loiter;
    if (s == "REPEAT")      return cmd_type::repeat;
    if (s == "RTB")         return cmd_type::rtb;
    throw std::runtime_error("Unknown command type: " + s);
}

// ── Params serialisation ──────────────────────────────────────────────────────

static json params_to_json(const mission_item& item)
{
    json p = json::object();   // always an object, never null
    std::visit([&](const auto& params)
    {
        using T = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<T, move_to_wp_params>)
        {
            p["wp_id"]     = params.wp_id;
            p["arrival_m"] = params.arrival_m;
        }
        else if constexpr (std::is_same_v<T, search_area_params>)
        {
            p["radius_m"]  = params.radius_m;
            p["spacing_m"] = params.spacing_m;
        }
        else if constexpr (std::is_same_v<T, loiter_params>)
        {
            p["duration_s"] = params.duration_s;
        }
        else if constexpr (std::is_same_v<T, repeat_params>)
        {
            p["count"] = params.count;
        }
        // rtb_params: empty object — no fields needed
    }, item.params);
    return p;
}

static cmd_params params_from_json(cmd_type type, const json& p)
{
    switch (type)
    {
        case cmd_type::move_to_wp:
            return move_to_wp_params{
                p.value("wp_id",     std::string{}),
                p.value("arrival_m", 10.0)
            };
        case cmd_type::search_area:
            return search_area_params{
                p.value("radius_m",  200.0),
                p.value("spacing_m",  40.0)
            };
        case cmd_type::loiter:
            return loiter_params{ p.value("duration_s", 30.0) };
        case cmd_type::repeat:
            return repeat_params{ p.value("count", 2) };
        case cmd_type::rtb:
            return rtb_params{};
    }
    return rtb_params{};
}

// ── Public API ────────────────────────────────────────────────────────────────

bool mission_io::save(const mission& m, const std::string& path)
{
    try
    {
        json j;
        j["version"] = 1;
        j["name"]    = m.name;

        // Waypoints
        json wps = json::array();
        for (auto& [id, wp] : m.waypoints)
            wps.push_back({ {"id", wp.id}, {"lat", wp.lat}, {"lon", wp.lon} });
        j["waypoints"] = wps;

        // Geofence
        json verts = json::array();
        for (auto& [lat, lon] : m.fence.verts)
            verts.push_back({ {"lat", lat}, {"lon", lon} });
        j["geofence"] = { {"enabled", m.fence.enabled}, {"vertices", verts} };

        // Commands
        json cmds = json::array();
        for (auto& item : m.commands)
        {
            cmds.push_back({
                {"id",     item.id},
                {"type",   type_to_str(item.type)},
                {"params", params_to_json(item)}
            });
        }
        j["commands"] = cmds;

        std::ofstream f(path);
        if (!f) return false;
        f << j.dump(2) << '\n';
        return true;
    }
    catch (...) { return false; }
}

bool mission_io::load(mission& m, const std::string& path)
{
    try
    {
        std::ifstream f(path);
        if (!f) return false;

        json j;
        f >> j;

        if (j.value("version", 0) != 1) return false;

        mission tmp;
        tmp.name = j.value("name", std::string{"Untitled"});

        for (auto& jw : j.at("waypoints"))
        {
            waypoint wp;
            wp.id  = jw.at("id").get<std::string>();
            wp.lat = jw.at("lat").get<double>();
            wp.lon = jw.at("lon").get<double>();
            tmp.waypoints[wp.id] = wp;
        }

        auto& gf = j.at("geofence");
        tmp.fence.enabled = gf.value("enabled", true);
        for (auto& v : gf.at("vertices"))
            tmp.fence.verts.push_back({v.at("lat").get<double>(),
                                       v.at("lon").get<double>()});

        for (auto& jc : j.at("commands"))
        {
            mission_item item;
            item.id     = jc.at("id").get<std::string>();
            item.type   = str_to_type(jc.at("type").get<std::string>());
            item.params = params_from_json(item.type, jc.at("params"));
            tmp.commands.push_back(std::move(item));
        }

        m = std::move(tmp);
        return true;
    }
    catch (...) { return false; }
}
