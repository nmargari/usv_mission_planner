#include "mission_editor.h"
#include "core/config.h"
#include <raylib.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ── Construction ──────────────────────────────────────────────────────────────

mission_editor::mission_editor(mission& m, map_view& view)
    : mission_(m)
    , view_(view)
{}

// ── Tool ─────────────────────────────────────────────────────────────────────

void mission_editor::set_tool(editor_tool t)
{
    if (tool_ == editor_tool::draw_geofence && t != editor_tool::draw_geofence)
        in_progress_.clear();
    tool_ = t;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::string mission_editor::wp_under_mouse() const
{
    Vector2 mouse = GetMousePosition();
    for (auto& [id, wp] : mission_.waypoints)
    {
        Vector2 pos = view_.latlon_to_screen(wp.lat, wp.lon);
        float dx = mouse.x - pos.x;
        float dy = mouse.y - pos.y;
        if (std::sqrt(dx * dx + dy * dy) <= 12.f)
            return id;
    }
    return {};
}

std::string mission_editor::next_wp_id() const
{
    int n = static_cast<int>(mission_.waypoints.size()) + 1;
    char buf[32];
    while (true)
    {
        std::snprintf(buf, sizeof(buf), "WP_%02d", n);
        if (!mission_.waypoints.count(buf))
            return buf;
        ++n;
    }
}

void mission_editor::begin_rename(const std::string& wp_id)
{
    sel_id_   = wp_id;
    renaming_ = true;
    std::strncpy(rename_buf_, wp_id.c_str(), sizeof(rename_buf_) - 1);
    rename_buf_[sizeof(rename_buf_) - 1] = '\0';
}

void mission_editor::confirm_rename()
{
    renaming_ = false;

    std::string new_id = rename_buf_;
    if (new_id.empty() || new_id == sel_id_)
        return;

    // Reject duplicate names
    if (mission_.waypoints.count(new_id))
        return;

    auto node = mission_.waypoints.extract(sel_id_);
    node.key()         = new_id;
    node.mapped().id   = new_id;
    mission_.waypoints.insert(std::move(node));
    sel_id_ = new_id;
}

// ── Update ────────────────────────────────────────────────────────────────────

void mission_editor::handle_input()
{
    // Handle rename text input when the rename box is open
    if (renaming_)
    {
        int ch;
        while ((ch = GetCharPressed()) != 0)
        {
            int len = static_cast<int>(std::strlen(rename_buf_));
            if (len < 63 && ch >= 32)
            {
                rename_buf_[len]     = static_cast<char>(ch);
                rename_buf_[len + 1] = '\0';
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE))
        {
            int len = static_cast<int>(std::strlen(rename_buf_));
            if (len > 0)
                rename_buf_[len - 1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER))
            confirm_rename();
        if (IsKeyPressed(KEY_ESCAPE))
            renaming_ = false;
        return;   // block map interaction while typing
    }

    Vector2 mouse = GetMousePosition();
    if (mouse.y <= config::toolbar_h) return;

    // Right-click on a waypoint → delete it
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        std::string hit = wp_under_mouse();
        if (!hit.empty())
        {
            if (sel_id_ == hit) sel_id_.clear();
            mission_.waypoints.erase(hit);
        }
        return;
    }

    // Left click — only below the toolbar
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return;

    std::string hit = wp_under_mouse();

    if (!hit.empty())
    {
        // Click on existing waypoint → rename it
        begin_rename(hit);
        return;
    }

    if (tool_ == editor_tool::place_waypoint)
    {
        // Click on empty map → place new waypoint and immediately rename
        double lat, lon;
        view_.screen_to_latlon(mouse.x, mouse.y, lat, lon);

        waypoint wp;
        wp.id  = next_wp_id();
        wp.lat = lat;
        wp.lon = lon;

        mission_.waypoints[wp.id] = wp;

        begin_rename(wp.id);
    }
    else
    {
        // Click on empty map with no tool → deselect
        sel_id_.clear();
    }
}

void mission_editor::handle_geofence_tool()
{
    if (IsKeyPressed(KEY_ESCAPE))
    {
        in_progress_.clear();
        return;
    }

    Vector2 mouse = GetMousePosition();
    bool in_map = mouse.y > config::toolbar_h
               && mouse.x < GetScreenWidth() - config::panel_w;
    if (!in_map) return;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        double lat, lon;
        view_.screen_to_latlon(mouse.x, mouse.y, lat, lon);
        in_progress_.push_back({lat, lon});
        return;
    }

    // Right-click with ≥ 3 vertices → commit
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && in_progress_.size() >= 3)
    {
        mission_.fence.verts   = in_progress_;
        mission_.fence.enabled = true;
        in_progress_.clear();
    }
}

void mission_editor::update()
{
    if (tool_ == editor_tool::draw_geofence)
    {
        handle_geofence_tool();
        return;
    }
    handle_input();
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void mission_editor::draw_waypoints() const
{
    for (auto& [id, wp] : mission_.waypoints)
    {
        Vector2 pos = view_.latlon_to_screen(wp.lat, wp.lon);
        wp.px = pos.x;
        wp.py = pos.y;

        bool selected = (id == sel_id_);
        Color fill    = selected ? Color{255, 165,   0, 255}
                                 : Color{255,  60,  60, 255};

        DrawCircleV(pos, 8.f, fill);
        DrawCircleLines(static_cast<int>(pos.x), static_cast<int>(pos.y), 8, WHITE);

        // Name label to the right of the marker
        DrawText(id.c_str(),
                 static_cast<int>(pos.x) + 12,
                 static_cast<int>(pos.y) - 8,
                 14, WHITE);
    }
}

void mission_editor::draw_rename_panel()
{
    if (!renaming_ || sel_id_.empty())
        return;

    auto it = mission_.waypoints.find(sel_id_);
    if (it == mission_.waypoints.end()) return;

    const waypoint& wp = it->second;
    Vector2 pos = view_.latlon_to_screen(wp.lat, wp.lon);

    const int pw = 240;
    const int ph = 68;
    // Right edge of the map area — never overlap the command panel
    const int map_right = GetScreenWidth() - config::panel_w;

    // Place the box to the right of the waypoint marker, then clamp into map area
    int px = static_cast<int>(pos.x) + 18;
    int py = static_cast<int>(pos.y) - ph / 2;
    px = std::clamp(px, 8, map_right - pw - 8);
    py = std::clamp(py, config::toolbar_h + 8, GetScreenHeight() - ph - 8);

    DrawRectangle(px, py, pw, ph, Color{18, 18, 32, 235});
    DrawRectangleLinesEx({(float)px, (float)py, (float)pw, (float)ph},
                         1.5f, Color{100, 100, 160, 255});

    DrawText("Rename waypoint (Enter to confirm)",
             px + 8, py + 8, 12, LIGHTGRAY);

    // Text box
    DrawRectangle(px + 8, py + 28, pw - 16, 28, Color{35, 35, 55, 255});
    DrawRectangleLinesEx({(float)(px + 8), (float)(py + 28), (float)(pw - 16), 28.f},
                         1.f, Color{80, 160, 255, 255});
    DrawText(rename_buf_, px + 14, py + 34, 15, WHITE);

    // Blinking cursor
    if (static_cast<int>(GetTime() * 2) % 2 == 0)
    {
        int cx = px + 14 + MeasureText(rename_buf_, 15);
        DrawLine(cx, py + 31, cx, py + 53, WHITE);
    }
}

void mission_editor::draw_geofence() const
{
    // ── Committed geofence ────────────────────────────────────────────────────
    if (mission_.fence.enabled && mission_.fence.verts.size() >= 3)
    {
        std::vector<Vector2> pts;
        pts.reserve(mission_.fence.verts.size());
        for (auto& [lat, lon] : mission_.fence.verts)
            pts.push_back(view_.latlon_to_screen(lat, lon));

        // Semi-transparent fill (fan triangulation)
        Color fill = {50, 200, 80, 28};
        for (std::size_t i = 1; i + 1 < pts.size(); ++i)
            DrawTriangle(pts[0], pts[i], pts[i + 1], fill);

        // Outline + vertex dots
        Color line = {60, 220, 90, 210};
        for (std::size_t i = 0; i < pts.size(); ++i)
        {
            Vector2 a = pts[i];
            Vector2 b = pts[(i + 1) % pts.size()];
            DrawLineEx(a, b, 2.f, line);
            DrawCircleV(a, 4.f, line);
        }
    }

    // ── In-progress polygon ───────────────────────────────────────────────────
    if (in_progress_.empty()) return;

    std::vector<Vector2> pts;
    pts.reserve(in_progress_.size());
    for (auto& [lat, lon] : in_progress_)
        pts.push_back(view_.latlon_to_screen(lat, lon));

    Color col = {120, 240, 120, 200};

    for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        DrawLineEx(pts[i], pts[i + 1], 2.f, col);

    for (auto& p : pts)
        DrawCircleV(p, 5.f, col);

    // Highlight first vertex when closeable (shows user where to right-click)
    if (pts.size() >= 3)
        DrawCircleLines(static_cast<int>(pts[0].x), static_cast<int>(pts[0].y),
                        9, Color{200, 255, 200, 220});

    // Preview line from last vertex to mouse cursor
    Vector2 mouse = GetMousePosition();
    if (mouse.y > config::toolbar_h && mouse.x < GetScreenWidth() - config::panel_w)
        DrawLineEx(pts.back(), mouse, 1.5f, Color{120, 240, 120, 90});
}

void mission_editor::draw()
{
    draw_geofence();
    draw_waypoints();
    draw_rename_panel();
}
