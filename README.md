# git-get
A TUI tool for searching github repositories written in C++ using ncurses. 

---
## installation
```bash
# clone the repo
git clone https://github.com/type1arc/git-get.git
# build
cd git-get && cmake -S . -B build && cmake --build build -j
# copy binary to /usr/bin/
sudo cp build/gg /usr/bin/
# clean-up
rm -rf git-get/
```

## usage
type `gg` in your shell, the tui will open.
press `i`, to enter insert mode, the cursor will jump to the search bar
use `j`, `k` to navigate between entries. 
