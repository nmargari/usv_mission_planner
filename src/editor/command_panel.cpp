#include "command_panel.h"
#include <raylib.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <variant>

// ── Palette ───────────────────────────────────────────────────────────────────

static constexpr Color k_bg         = {18,  20,  36, 245};
static constexpr Color k_block_bg   = {30,  32,  52, 255};
static constexpr Color k_block_sel  = {42,  48,  80, 255};
static constexpr Color k_border     = {60,  65, 100, 255};
static constexpr Color k_handle     = {55,  60,  90, 255};
static constexpr Color k_del_btn    = {130,  35,  35, 255};
static constexpr Color k_add_btn    = {38,  80, 140, 255};
static constexpr Color k_edit_bg    = {28,  30,  50, 255};
static constexpr Color k_edit_bdr   = {80, 160, 255, 255};
static constexpr Color k_ghost      = {80,  90, 140,  80};

// ── Type name strings ─────────────────────────────────────────────────────────

static const char* type_name(cmd_type t)
{
    switch (t)
    {
        case cmd_type::move_to_wp:   return "MOVE_TO_WP";
        case cmd_type::search_area:  return "SEARCH_AREA";
        case cmd_type::loiter:       return "LOITER";
        case cmd_type::repeat:       return "REPEAT";
        case cmd_type::rtb:          return "RTB";
    }
    return "?";
}

// ── Construction ─────────────────────────────────────────────────────────────

command_panel::command_panel(mission& m, int x, int y, int w, int h)
    : mission_(m), x_(x), y_(y), w_(w), h_(h)
{}

// ── Layout helpers ────────────────────────────────────────────────────────────

float command_panel::block_top(int display_idx) const
{
    return static_cast<float>(y_ + k_header_h)
         + display_idx * (k_block_h + k_gap)
         - scroll_offset_;
}

int command_panel::compute_target_idx() const
{
    float dragged_center = drag_mouse_y_ - drag_anchor_y_ + k_block_h / 2.f;
    float rel = dragged_center - static_cast<float>(y_ + k_header_h) + scroll_offset_;
    int t = static_cast<int>(rel / (k_block_h + k_gap));
    int n = static_cast<int>(mission_.commands.size());
    return std::clamp(t, 0, n - 1);
}

void command_panel::clamp_scroll()
{
    int n = static_cast<int>(mission_.commands.size());
    float content_h = static_cast<float>(n * (k_block_h + k_gap));
    float visible_h = static_cast<float>(h_ - k_header_h - k_addbtn_h);
    float max_scroll = std::max(0.f, content_h - visible_h);
    scroll_offset_ = std::clamp(scroll_offset_, 0.f, max_scroll);
}

std::string command_panel::next_item_id() const
{
    int n = static_cast<int>(mission_.commands.size()) + 1;
    char buf[32];
    while (true)
    {
        std::snprintf(buf, sizeof(buf), "cmd_%03d", n);
        bool used = false;
        for (auto& item : mission_.commands)
            if (item.id == buf) { used = true; break; }
        if (!used) return buf;
        ++n;
    }
}

// ── Inline param editing ──────────────────────────────────────────────────────

void command_panel::begin_edit(int item_idx, int field_idx)
{
    edit_item_idx_  = item_idx;
    edit_field_idx_ = field_idx;
    editing_        = true;

    auto& item = mission_.commands[item_idx];
    char buf[128] = {};

    std::visit([&](auto& p)
    {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, move_to_wp_params>)
        {
            if (field_idx == 0) std::snprintf(buf, sizeof(buf), "%s", p.wp_id.c_str());
            if (field_idx == 1) std::snprintf(buf, sizeof(buf), "%.1f", p.arrival_m);
        }
        else if constexpr (std::is_same_v<T, search_area_params>)
        {
            if (field_idx == 0) std::snprintf(buf, sizeof(buf), "%.1f", p.radius_m);
            if (field_idx == 1) std::snprintf(buf, sizeof(buf), "%.1f", p.spacing_m);
        }
        else if constexpr (std::is_same_v<T, loiter_params>)
        {
            if (field_idx == 0) std::snprintf(buf, sizeof(buf), "%.1f", p.duration_s);
        }
        else if constexpr (std::is_same_v<T, repeat_params>)
        {
            if (field_idx == 0) std::snprintf(buf, sizeof(buf), "%d", p.count);
        }
    }, item.params);

    std::snprintf(edit_buf_, sizeof(edit_buf_), "%s", buf);
}

void command_panel::confirm_edit()
{
    if (edit_item_idx_ < 0 || edit_item_idx_ >= static_cast<int>(mission_.commands.size()))
    {
        editing_ = false;
        return;
    }

    auto& item = mission_.commands[edit_item_idx_];
    std::visit([&](auto& p)
    {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, move_to_wp_params>)
        {
            if (edit_field_idx_ == 0) p.wp_id     = edit_buf_;
            if (edit_field_idx_ == 1) p.arrival_m = std::atof(edit_buf_);
        }
        else if constexpr (std::is_same_v<T, search_area_params>)
        {
            if (edit_field_idx_ == 0) p.radius_m  = std::atof(edit_buf_);
            if (edit_field_idx_ == 1) p.spacing_m = std::atof(edit_buf_);
        }
        else if constexpr (std::is_same_v<T, loiter_params>)
        {
            if (edit_field_idx_ == 0) p.duration_s = std::atof(edit_buf_);
        }
        else if constexpr (std::is_same_v<T, repeat_params>)
        {
            if (edit_field_idx_ == 0) p.count = std::atoi(edit_buf_);
        }
    }, item.params);

    editing_ = false;
}

void command_panel::handle_edit_input()
{
    int ch;
    while ((ch = GetCharPressed()) != 0)
    {
        int len = static_cast<int>(std::strlen(edit_buf_));
        if (len < 127 && ch >= 32)
        {
            edit_buf_[len]     = static_cast<char>(ch);
            edit_buf_[len + 1] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = static_cast<int>(std::strlen(edit_buf_));
        if (len > 0) edit_buf_[len - 1] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER))  confirm_edit();
    if (IsKeyPressed(KEY_ESCAPE)) editing_ = false;
}

// ── Input ─────────────────────────────────────────────────────────────────────

void command_panel::handle_scroll(Vector2 mouse)
{
    bool in_panel = mouse.x >= x_ && mouse.x < x_ + w_
                 && mouse.y >= y_ && mouse.y < y_ + h_;
    if (!in_panel) return;
    scroll_offset_ -= GetMouseWheelMove() * 30.f;
    clamp_scroll();
}

void command_panel::handle_drag(Vector2 mouse)
{
    drag_mouse_y_ = mouse.y;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        finish_drag();
}

void command_panel::finish_drag()
{
    int target = compute_target_idx();
    if (target != drag_idx_)
    {
        auto item = mission_.commands[drag_idx_];
        mission_.commands.erase(mission_.commands.begin() + drag_idx_);
        mission_.commands.insert(mission_.commands.begin() + target, item);
    }
    drag_idx_ = -1;
}

void command_panel::handle_clicks(Vector2 mouse)
{
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    int n = static_cast<int>(mission_.commands.size());

    // ── Add-command buttons (fixed at bottom) ─────────────────────────────────
    int add_y = y_ + h_ - k_addbtn_h + 6;
    float bw = (w_ - 10.f) / 5.f;
    const cmd_type add_types[] = {
        cmd_type::move_to_wp, cmd_type::search_area, cmd_type::loiter,
        cmd_type::repeat,     cmd_type::rtb
    };
    for (int i = 0; i < 5; ++i)
    {
        Rectangle r = {static_cast<float>(x_) + 5.f + i * bw, static_cast<float>(add_y),
                       bw - 4.f, 32.f};
        if (CheckCollisionPointRec(mouse, r))
        {
            mission_item item;
            item.id   = next_item_id();
            item.type = add_types[i];
            switch (add_types[i])
            {
                case cmd_type::move_to_wp:  item.params = move_to_wp_params{};  break;
                case cmd_type::search_area: item.params = search_area_params{}; break;
                case cmd_type::loiter:      item.params = loiter_params{};      break;
                case cmd_type::repeat:      item.params = repeat_params{};      break;
                case cmd_type::rtb:         item.params = rtb_params{};         break;
            }
            // RTB always goes last; others insert before RTB if present
            auto it = mission_.commands.end();
            if (add_types[i] != cmd_type::rtb)
            {
                for (auto jt = mission_.commands.begin(); jt != mission_.commands.end(); ++jt)
                    if (jt->type == cmd_type::rtb) { it = jt; break; }
            }
            mission_.commands.insert(it, item);
            return;
        }
    }

    // ── Command blocks ────────────────────────────────────────────────────────
    for (int i = 0; i < n; ++i)
    {
        // Compute display_idx considering any ongoing drag (none here since drag
        // was already resolved; this handles fresh clicks)
        float by = block_top(i);
        if (by + k_block_h < y_ + k_header_h || by > y_ + h_ - k_addbtn_h) continue;

        // Delete button (top-right of block)
        Rectangle del_r = {static_cast<float>(x_ + w_ - 30), by + 4.f, 24.f, 22.f};
        if (CheckCollisionPointRec(mouse, del_r))
        {
            mission_.commands.erase(mission_.commands.begin() + i);
            if (edit_item_idx_ == i) editing_ = false;
            return;
        }

        // Drag handle (left strip)
        Rectangle handle_r = {static_cast<float>(x_), by, 24.f, static_cast<float>(k_block_h)};
        if (CheckCollisionPointRec(mouse, handle_r))
        {
            drag_idx_      = i;
            drag_mouse_y_  = mouse.y;
            drag_anchor_y_ = mouse.y - by;
            return;
        }

        // Param field 0 (first value row)
        Rectangle p0 = {static_cast<float>(x_ + 80), by + 26.f, static_cast<float>(w_ - 116), 20.f};
        if (CheckCollisionPointRec(mouse, p0) && mission_.commands[i].type != cmd_type::rtb)
        {
            begin_edit(i, 0);
            return;
        }

        // Param field 1 (second value row, if present)
        bool has_two = (mission_.commands[i].type == cmd_type::move_to_wp
                     || mission_.commands[i].type == cmd_type::search_area);
        if (has_two)
        {
            Rectangle p1 = {static_cast<float>(x_ + 80), by + 50.f,
                            static_cast<float>(w_ - 116), 20.f};
            if (CheckCollisionPointRec(mouse, p1))
            {
                begin_edit(i, 1);
                return;
            }
        }
    }

    // Click outside everything → cancel edit
    editing_ = false;
}

void command_panel::update()
{
    Vector2 mouse = GetMousePosition();
    handle_scroll(mouse);

    if (editing_)
    {
        handle_edit_input();
        // Cancel edit if user clicks outside the panel
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
            && (mouse.x < x_ || mouse.x >= x_ + w_))
            editing_ = false;
        return;
    }

    if (drag_idx_ >= 0)
    {
        handle_drag(mouse);
        return;
    }

    handle_clicks(mouse);
}

// ── Draw ──────────────────────────────────────────────────────────────────────

void command_panel::draw_block(int item_idx, int display_idx, bool is_dragging)
{
    const mission_item& item = mission_.commands[item_idx];

    float by   = is_dragging ? drag_mouse_y_ - drag_anchor_y_
                             : block_top(display_idx);
    float bx   = static_cast<float>(x_);
    float bw   = static_cast<float>(w_);
    float bh   = static_cast<float>(k_block_h);

    // Clip to list area
    if (!is_dragging && (by + bh < y_ + k_header_h || by > y_ + h_ - k_addbtn_h))
        return;

    Color bg = is_dragging ? Color{50, 60, 100, 210} : k_block_bg;
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                  static_cast<int>(bw), static_cast<int>(bh), bg);
    DrawRectangleLinesEx({bx, by, bw, bh}, 1.f, k_border);

    // Drag handle strip
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), 20, static_cast<int>(bh), k_handle);
    DrawText("=", static_cast<int>(bx) + 5, static_cast<int>(by + bh / 2.f) - 7, 14, LIGHTGRAY);

    // Index
    char idx_buf[16];
    std::snprintf(idx_buf, sizeof(idx_buf), "%d", item_idx + 1);
    DrawText(idx_buf, static_cast<int>(bx) + 24, static_cast<int>(by) + 5, 12, DARKGRAY);

    // Type name
    DrawText(type_name(item.type),
             static_cast<int>(bx) + 36, static_cast<int>(by) + 5, 14, WHITE);

    // Delete button
    DrawRectangle(static_cast<int>(bx + bw) - 30, static_cast<int>(by) + 4, 24, 22, k_del_btn);
    DrawText("x", static_cast<int>(bx + bw) - 23, static_cast<int>(by) + 7, 13, WHITE);

    // Parameter rows
    auto draw_param = [&](int field_idx, float row_y,
                          const char* label, const char* value)
    {
        DrawText(label, static_cast<int>(bx) + 26, static_cast<int>(row_y) + 2, 12, GRAY);

        bool is_editing = editing_
                       && edit_item_idx_  == item_idx
                       && edit_field_idx_ == field_idx;

        Rectangle val_r = {bx + 80.f, row_y, bw - 116.f, 20.f};

        if (is_editing)
        {
            DrawRectangleRec(val_r, k_edit_bg);
            DrawRectangleLinesEx(val_r, 1.f, k_edit_bdr);
            DrawText(edit_buf_, static_cast<int>(val_r.x) + 4,
                     static_cast<int>(val_r.y) + 3, 13, WHITE);
            if (static_cast<int>(GetTime() * 2) % 2 == 0)
            {
                int cx = static_cast<int>(val_r.x) + 4
                       + MeasureText(edit_buf_, 13);
                DrawLine(cx, static_cast<int>(val_r.y) + 2,
                         cx, static_cast<int>(val_r.y) + 18, WHITE);
            }
        }
        else
        {
            DrawRectangleRec(val_r, Color{24, 26, 44, 200});
            DrawRectangleLinesEx(val_r, 1.f, Color{55, 58, 88, 255});
            DrawText(value, static_cast<int>(val_r.x) + 4,
                     static_cast<int>(val_r.y) + 3, 13, LIGHTGRAY);
        }
    };

    char val0[64] = {}, val1[64] = {};
    const char *lbl0 = "", *lbl1 = "";

    std::visit([&](const auto& p)
    {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, move_to_wp_params>)
        {
            lbl0 = "wp:";
            lbl1 = "arr:";
            std::snprintf(val0, sizeof(val0), "%s",  p.wp_id.c_str());
            std::snprintf(val1, sizeof(val1), "%.1f m", p.arrival_m);
        }
        else if constexpr (std::is_same_v<T, search_area_params>)
        {
            lbl0 = "rad:";
            lbl1 = "spc:";
            std::snprintf(val0, sizeof(val0), "%.1f m", p.radius_m);
            std::snprintf(val1, sizeof(val1), "%.1f m", p.spacing_m);
        }
        else if constexpr (std::is_same_v<T, loiter_params>)
        {
            lbl0 = "dur:";
            std::snprintf(val0, sizeof(val0), "%.1f s", p.duration_s);
        }
        else if constexpr (std::is_same_v<T, repeat_params>)
        {
            lbl0 = "n:";
            std::snprintf(val0, sizeof(val0), "%d×", p.count);
        }
        // rtb_params: no fields
    }, item.params);

    if (lbl0[0] != '\0') draw_param(0, by + 26.f, lbl0, val0);
    if (lbl1[0] != '\0') draw_param(1, by + 50.f, lbl1, val1);
}

void command_panel::draw_add_buttons()
{
    int add_y = y_ + h_ - k_addbtn_h;

    // Separator
    DrawLine(x_, add_y, x_ + w_, add_y, k_border);
    DrawRectangle(x_, add_y, w_, k_addbtn_h, Color{14, 16, 28, 255});

    float bw = (w_ - 10.f) / 5.f;
    const char* labels[] = {"MOVE", "SRCH", "LOITER", "RPT", "RTB"};
    for (int i = 0; i < 5; ++i)
    {
        float bx = static_cast<float>(x_) + 5.f + i * bw;
        float by = static_cast<float>(add_y) + 6.f;
        Vector2 mouse = GetMousePosition();
        bool hover = CheckCollisionPointRec(mouse, {bx, by, bw - 4.f, 32.f});
        Color bg = hover ? Color{55, 100, 180, 255} : k_add_btn;
        DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                      static_cast<int>(bw - 4.f), 32, bg);
        DrawRectangleLinesEx({bx, by, bw - 4.f, 32.f}, 1.f, k_border);
        int tw = MeasureText(labels[i], 11);
        DrawText(labels[i],
                 static_cast<int>(bx + (bw - 4.f) / 2.f - tw / 2.f),
                 static_cast<int>(by) + 10, 11, WHITE);
    }
}

void command_panel::draw()
{
    int n = static_cast<int>(mission_.commands.size());

    // Panel background
    DrawRectangle(x_, y_, w_, h_, k_bg);
    DrawRectangleLinesEx({static_cast<float>(x_), static_cast<float>(y_),
                          static_cast<float>(w_), static_cast<float>(h_)},
                         1.f, k_border);

    // Title
    DrawRectangle(x_, y_, w_, k_header_h, Color{22, 24, 42, 255});
    DrawLine(x_, y_ + k_header_h, x_ + w_, y_ + k_header_h, k_border);
    DrawText("Commands", x_ + 10, y_ + 10, 15, Color{180, 185, 220, 255});
    char count_buf[16];
    std::snprintf(count_buf, sizeof(count_buf), "%d", n);
    DrawText(count_buf, x_ + w_ - MeasureText(count_buf, 13) - 10, y_ + 12, 13, DARKGRAY);

    // Scissor to list area so blocks don't overdraw title/add strip
    BeginScissorMode(x_, y_ + k_header_h, w_, h_ - k_header_h - k_addbtn_h);

    int target = drag_idx_ >= 0 ? compute_target_idx() : -1;

    for (int i = 0; i < n; ++i)
    {
        if (i == drag_idx_) continue;   // draw dragged item last (on top)

        // Compute display index: shift items to make room for drag target
        int di = i;
        if (drag_idx_ >= 0)
        {
            if (i >= target && i < drag_idx_) di++;
            if (i > drag_idx_ && i <= target) di--;
        }
        draw_block(i, di, false);
    }

    // Ghost slot showing where the dragged block will land
    if (drag_idx_ >= 0)
    {
        float slot_y = block_top(target);
        DrawRectangle(x_, static_cast<int>(slot_y), w_, k_block_h, k_ghost);
        DrawRectangleLinesEx({static_cast<float>(x_), slot_y,
                              static_cast<float>(w_), static_cast<float>(k_block_h)},
                             1.5f, Color{100, 120, 200, 180});
    }

    EndScissorMode();

    // Dragged block on top (not clipped)
    if (drag_idx_ >= 0)
        draw_block(drag_idx_, -1, true);

    draw_add_buttons();
}
