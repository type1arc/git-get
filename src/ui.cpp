#include "ui.hpp"

#include <ncurses.h>

#include <algorithm>

void draw_ui(const ui_state& state, const github_searcher::state_snapshot& snap, int spinner_frame)
{
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);
    if (rows < 3 || cols < 10)
    {
        erase();
        refresh();
        return;
    }

    erase();

    attron(A_REVERSE | A_BOLD);
    mvhline(0, 0, ' ', cols);
    mvaddnstr(0, 1, " GitHub Repository Search ", cols - 2);
    const std::string mode_label = state.mode == input_mode::insert ? " INSERT " : " NORMAL ";
    if (cols > static_cast<int>(mode_label.size()) + 2)
    {
        mvaddnstr(0, cols - static_cast<int>(mode_label.size()) - 1, mode_label.c_str(),
                  static_cast<int>(mode_label.size()));
    }
    attroff(A_REVERSE | A_BOLD);

    const int max_query = std::max(0, cols - 3);
    std::string shown_query = state.query;
    if (static_cast<int>(shown_query.size()) > max_query)
    {
        shown_query = shown_query.substr(shown_query.size() - max_query);
    }
    if (state.mode == input_mode::insert)
    {
        attron(A_BOLD);
    }
    mvaddnstr(1, 0, ("> " + shown_query).c_str(), cols - 1);
    if (state.mode == input_mode::insert)
    {
        attroff(A_BOLD);
        int cursor_col = std::min(cols - 1, 2 + static_cast<int>(shown_query.size()));
        move(1, cursor_col);
        curs_set(1);
    }
    else
    {
        curs_set(0);
    }

    const int status_row = 2;
    mvhline(status_row, 0, ACS_HLINE, cols);
    if (!snap.error.empty())
    {
        attron(COLOR_PAIR(4));
        mvaddnstr(status_row, 1, snap.error.c_str(), cols - 2);
        attroff(COLOR_PAIR(4));
    }
    else if (snap.busy)
    {
        static const char spin[] = "|/-\\";
        std::string status = "Searching GitHub ";
        status += spin[spinner_frame % 4];
        mvaddnstr(status_row, 1, status.c_str(), cols - 2);
    }
    else if (!snap.results.empty())
    {
        std::string status = std::to_string(snap.results.size()) + " repositories";
        mvaddnstr(status_row, 1, status.c_str(), cols - 2);
    }
    else if (!state.query.empty())
    {
        if (snap.searched_query == state.query)
        {
            attron(A_DIM);
            mvaddnstr(status_row, 1, "no repositories found", cols - 2);
            attroff(A_DIM);
        }
        else
        {
            mvaddnstr(status_row, 1, "press Enter to search", cols - 2);
        }
    }
    else
    {
        attron(A_DIM);
        mvaddnstr(status_row, 1, "type a repository or owner name, then press Enter", cols - 2);
        attroff(A_DIM);
    }

    const int list_top = 3;
    const int hint_row = rows - 1;
    const int visible_rows = std::max(0, hint_row - list_top);
    const int count = static_cast<int>(snap.results.size());
    const int selected = std::min(state.selected, std::max(0, count - 1));
    const int scroll = std::min(state.scroll, std::max(0, count - 1));

    for (int i = 0; i < visible_rows; ++i)
    {
        int index = scroll + i;
        if (index >= count)
        {
            break;
        }
        int row = list_top + i;
        const repository& repo = snap.results[index];
        bool is_selected = index == selected;

        if (is_selected)
        {
            attron(A_REVERSE | A_BOLD);
            mvhline(row, 0, ' ', cols);
        }

        std::string stars = "* " + std::to_string(repo.stars);
        int stars_col = std::max(1, cols - static_cast<int>(stars.size()) - 1);
        int name_width = std::max(0, stars_col - 2);
        mvaddnstr(row, 1, repo.full_name.c_str(), name_width);
        mvaddnstr(row, stars_col, stars.c_str(), cols - stars_col - 1);

        if (is_selected)
        {
            attroff(A_REVERSE | A_BOLD);
        }
    }

    attron(A_REVERSE);
    mvhline(hint_row, 0, ' ', cols);
    const char* hint = state.mode == input_mode::insert
                           ? " INSERT   Enter: search   Esc: normal   Ctrl-U: clear query "
                           : " NORMAL   i: search bar   j/k: move   Enter: print & quit   q: quit ";
    mvaddnstr(hint_row, 0, hint, cols - 1);
    attroff(A_REVERSE);

    refresh();
}

void pop_utf8(std::string& value)
{
    if (value.empty())
    {
        return;
    }
    value.pop_back();
    while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80)
    {
        value.pop_back();
    }
}

void clamp_selection(ui_state& state, int count)
{
    if (count > 0)
    {
        state.selected = std::min(state.selected, count - 1);
    }
    else
    {
        state.selected = 0;
    }
    state.scroll = std::min(state.scroll, std::max(0, count - 1));
}

void adjust_scroll(ui_state& state)
{
    int visible_rows = std::max(0, getmaxy(stdscr) - 4);
    if (state.selected < state.scroll)
    {
        state.scroll = state.selected;
    }
    if (visible_rows > 0 && state.selected >= state.scroll + visible_rows)
    {
        state.scroll = state.selected - visible_rows + 1;
    }
    state.scroll = std::max(0, state.scroll);
}
