#include <ncurses.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <locale.h>
#include <string>

#include "github.hpp"
#include "ui.hpp"

namespace
{

constexpr int spinner_delay_ms = 40;

class key_handler
{
public:
    key_handler(ui_state& ui, github_searcher& searcher) : state(ui), search(searcher)
    {
    }

    bool handle(int ch, const github_searcher::state_snapshot& snap)
    {
        if (ch == 3)
        {
            running = false;
            return false;
        }
        if (state.mode == input_mode::insert)
        {
            handle_insert(ch, snap);
        }
        else
        {
            handle_normal(ch, snap);
        }
        return running;
    }

    const std::string& chosen() const
    {
        return chosen_repo;
    }

private:
    void enter_insert()
    {
        state.mode = input_mode::insert;
    }

    void handle_insert(int ch, const github_searcher::state_snapshot& snap)
    {
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        {
            if (!state.query.empty())
            {
                search.request(state.query);
                state.selected = 0;
                state.scroll = 0;
            }
        }
        else if (ch == 27)
        {
            state.mode = input_mode::normal;
        }
        else if (ch == '\t')
        {
            if (!snap.results.empty())
            {
                state.mode = input_mode::normal;
            }
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
        {
            pop_utf8(state.query);
        }
        else if (ch == 21)
        {
            state.query.clear();
        }
        else if (ch >= 32 && ch != 127)
        {
            state.query += static_cast<char>(ch);
        }
    }

    void handle_normal(int ch, const github_searcher::state_snapshot& snap)
    {
        if (ch == 'q')
        {
            running = false;
        }
        else if (ch == 27)
        {
            // Esc stays in normal mode, like vim.
        }
        else if (ch == 'i' || ch == 'a' || ch == '/')
        {
            enter_insert();
        }
        else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER)
        {
            if (!snap.results.empty())
            {
                chosen_repo = snap.results[state.selected].full_name;
                running = false;
            }
        }
        else if (ch == 'j' || ch == KEY_DOWN)
        {
            if (state.selected + 1 < static_cast<int>(snap.results.size()))
            {
                ++state.selected;
            }
        }
        else if (ch == 'k' || ch == KEY_UP)
        {
            if (state.selected > 0)
            {
                --state.selected;
            }
        }
        else if (ch == 'g' || ch == KEY_HOME)
        {
            state.selected = 0;
        }
        else if (ch == 'G' || ch == KEY_END)
        {
            state.selected = std::max(0, static_cast<int>(snap.results.size()) - 1);
        }
        else if (ch == 4 || ch == KEY_NPAGE)
        {
            state.selected = std::min(std::max(0, static_cast<int>(snap.results.size()) - 1),
                                      state.selected + 10);
        }
        else if (ch == 21 || ch == KEY_PPAGE)
        {
            state.selected = std::max(0, state.selected - 10);
        }
    }

    ui_state& state;
    github_searcher& search;
    bool running = true;
    std::string chosen_repo;
};

}  // namespace

int main()
{
    setlocale(LC_ALL, "");
    github_global_init();

    FILE* tty = std::fopen("/dev/tty", "r+");
    if (!tty)
    {
        std::fprintf(stderr, "repo-search: no terminal available\n");
        github_global_cleanup();
        return 1;
    }

    SCREEN* screen = newterm(nullptr, tty, tty);
    if (!screen)
    {
        std::fprintf(stderr, "repo-search: failed to initialize terminal\n");
        std::fclose(tty);
        github_global_cleanup();
        return 1;
    }
    set_term(screen);

    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(0);

    start_color();
    use_default_colors();
    init_pair(4, COLOR_RED, -1);

    github_searcher searcher;
    ui_state ui;
    key_handler keys(ui, searcher);
    int spinner_frame = 0;
    bool running = true;

    while (running)
    {
        bool fresh = searcher.take_ready();
        github_searcher::state_snapshot snap = searcher.snapshot();

        if (fresh && !snap.results.empty() && ui.mode == input_mode::insert)
        {
            ui.mode = input_mode::normal;
        }

        clamp_selection(ui, static_cast<int>(snap.results.size()));
        draw_ui(ui, snap, spinner_frame);

        int ch;
        while ((ch = getch()) != ERR)
        {
            if (!keys.handle(ch, snap))
            {
                running = false;
                break;
            }
        }

        adjust_scroll(ui);

        ++spinner_frame;
        napms(spinner_delay_ms);
    }

    std::string chosen = keys.chosen();

    endwin();
    delscreen(screen);
    std::fclose(tty);
    github_global_cleanup();

    if (!chosen.empty())
    {
        std::cout << chosen << '\n' << std::flush;
    }
    return 0;
}
