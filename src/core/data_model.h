#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

// ── Waypoint ──────────────────────────────────────────────────────────────────

struct waypoint
{
    std::string   id;
    double        lat = 0.0;
    double        lon = 0.0;
    mutable float px  = 0.f;   // screen pixel cache — updated each draw
    mutable float py  = 0.f;
};

// ── Mission commands ──────────────────────────────────────────────────────────

enum class cmd_type { move_to_wp, search_area, loiter, repeat, rtb };

struct move_to_wp_params
{
    std::string wp_id;
    double      arrival_m = 10.0;
};

struct search_area_params
{
    double radius_m   = 200.0;
    double spacing_m  =  40.0;
};

struct loiter_params
{
    double duration_s = 30.0;
};

struct repeat_params
{
    int count = 2;   // repeats the NEXT command N times
};

struct rtb_params {};

using cmd_params = std::variant<
    move_to_wp_params,
    search_area_params,
    loiter_params,
    repeat_params,
    rtb_params
>;

struct mission_item
{
    std::string id;      // "cmd_001"
    cmd_type    type;
    cmd_params  params;
};

// ── Mission ───────────────────────────────────────────────────────────────────

struct mission
{
    std::unordered_map<std::string, waypoint> waypoints;
    std::vector<mission_item>                  commands;
    std::string                                name = "Untitled";
};
