#pragma once

#include <string>

#include "github.hpp"

enum class input_mode
{
    normal,
    insert
};

struct ui_state
{
    std::string query;
    input_mode mode = input_mode::normal;
    int selected = 0;
    int scroll = 0;
};

void draw_ui(const ui_state& state, const github_searcher::state_snapshot& snap, int spinner_frame);
void pop_utf8(std::string& value);
void clamp_selection(ui_state& state, int count);
void adjust_scroll(ui_state& state);
