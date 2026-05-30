#pragma once

#include <string>
#include <unordered_map>

struct waypoint
{
    std::string   id;
    double        lat = 0.0;
    double        lon = 0.0;
    mutable float px  = 0.f;   // screen pixel cache — updated each draw
    mutable float py  = 0.f;
};

struct mission
{
    std::unordered_map<std::string, waypoint> waypoints;
    std::string name = "Untitled";
};
