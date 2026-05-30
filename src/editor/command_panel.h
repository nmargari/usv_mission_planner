#pragma once

#include "core/data_model.h"
#include <raylib.h>
#include <string>

// Right-side panel showing the ordered list of mission commands.
// Supports drag-and-drop reordering, inline parameter editing, and
// adding / deleting commands.
class command_panel
{
public:
    command_panel(mission& m, int x, int y, int w, int h);

    void update();
    void draw();

    void resize(int x, int y, int w, int h) { x_ = x; y_ = y; w_ = w; h_ = h; }

private:
    // Layout constants
    static constexpr int k_header_h  = 36;   // "Commands" title bar
    static constexpr int k_block_h   = 76;   // height of one command block
    static constexpr int k_gap       =  4;   // gap between blocks
    static constexpr int k_addbtn_h  = 44;   // add-buttons strip height

    // ── Helpers ───────────────────────────────────────────────────────────────
    float block_top(int display_idx) const;
    int   compute_target_idx() const;
    void  clamp_scroll();
    std::string next_item_id() const;

    // ── Input ─────────────────────────────────────────────────────────────────
    void handle_scroll(Vector2 mouse);
    void handle_drag(Vector2 mouse);
    void finish_drag();
    void handle_clicks(Vector2 mouse);
    void handle_edit_input();
    void begin_edit(int item_idx, int field_idx);
    void confirm_edit();

    // ── Draw ──────────────────────────────────────────────────────────────────
    void draw_block(int item_idx, int display_idx, bool is_dragging);
    void draw_add_buttons();

    // ── State ─────────────────────────────────────────────────────────────────
    mission& mission_;
    int x_, y_, w_, h_;
    float scroll_offset_ = 0.f;

    // drag
    int   drag_idx_       = -1;
    float drag_mouse_y_   = 0.f;
    float drag_anchor_y_  = 0.f;   // offset from block top to mouse on drag start

    // inline param editing
    int  edit_item_idx_  = -1;
    int  edit_field_idx_ = -1;   // 0 = first param, 1 = second param
    bool editing_        = false;
    char edit_buf_[128]  = {};
};
