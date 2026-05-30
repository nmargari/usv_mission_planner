#pragma once

#include <string>
#include "core/data_model.h"
#include "map/map_view.h"

enum class editor_tool { none, place_waypoint };

class mission_editor
{
public:
    mission_editor(mission& m, map_view& view);

    void update();
    void draw();

    void        set_tool(editor_tool t);
    editor_tool current_tool() const { return tool_; }

private:
    // Returns the id of the waypoint under the mouse, or "" if none
    std::string wp_under_mouse() const;

    std::string next_wp_id() const;

    void begin_rename(const std::string& wp_id);
    void confirm_rename();

    void handle_input();
    void draw_waypoints() const;
    void draw_rename_panel();

    mission&    mission_;
    map_view&   view_;
    editor_tool tool_    = editor_tool::none;
    std::string sel_id_;       // currently selected waypoint
    bool        renaming_ = false;
    char        rename_buf_[64] = {};
};
