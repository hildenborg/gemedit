# GEM Text Editor (Atari ST)

A multi-document text editor in C using GEM AES + VDI. Menus and dialogs are
built in code, so no `.RSC` file is needed.
Written mainly by using Claude AI.

## Source layout

| File | Responsibility |
|------|----------------|
| `main.c`      | start-up, `evnt_multi` loop, keyboard and AES message dispatch |
| `app.c/h`     | AES/VDI init, global state, list of open documents |
| `commands.c/h`| every action (files, edit, find/replace, all-file search, bookmarks) |
| `menu.c/h`    | menu tree built programmatically, enable/disable, Window and Bookmarks lists |
| `dialog.c/h`  | alerts, file selector, Find / Replace dialogs (TEDINFO fields) |
| `window.c/h`  | one GEM window per document: drawing, sliders, scrolling, mouse |
| `doc.c/h`     | document model: cursor, selection, editing, indent, undo/redo, search, file I/O (no GEM calls) |
| `bookmark.c/h`| ten bookmarks (F1..F10) and the rules that keep them on their text |
| `textbuf.c/h` | gap buffer + incrementally maintained line table |
| `undo.c/h`    | change stack (2048 records, typing/backspace runs merged) |
| `clip.c/h`    | internal clipboard shared by all documents |
| `gemcompat.h` | gemlib / Pure C differences in one place |

## Build

For m68k-atari-elf toolchain:

    ./vscode
    SHIFT-CTRL-B

For m68k-atari-mint-gcc + gemlib:

    make                                  # m68k-atari-mint-gcc + gemlib
    make CFLAGS="-O2 -DNO_FSEL_EXINPUT"   # for plain TOS 1.00

Pure C: add all `.c` files to a project, link with the AES/VDI/standard libs.

## Keys

| Key | Action | Key | Action |
|-----|--------|-----|--------|
| Ctrl-N | new document | Ctrl-X / C / V | cut / copy / paste |
| Ctrl-O | open | Ctrl-A | select all |
| Ctrl-S | save (asks for name if unknown) | Ctrl-F | find |
| **Shift-Ctrl-S** | **save all modified files** | **Shift-Ctrl-F** | **find in all open files** |
| Ctrl-Q | quit (Save / Ignore / Cancel for unsaved) | Ctrl-G | find next (in the mode last used) |
| Ctrl-Z / Ctrl-Y | undo / redo (Undo key also undoes) | Ctrl-H | find & replace |
| Ctrl-W | next window | **Shift-Ctrl-H** | **replace in all open files** |
| **Tab** | indent selected lines (inserts a tab otherwise) | **Shift-Tab** | remove one indent (selected lines or current line) |
| **F1..F10** | go to bookmark | **Shift+F1..F10** | set bookmark at the current line |

* **Replace dialog**: has a *Replace all occurrences* check box. Unchecked, *Replace* replaces the selected match and finds the next; checked, it replaces every match in the current file (one undo step).
* **Bookmarks menu**: lists the bookmarks that are set as `F<n> <document> Ln <line>` (selecting one jumps there) and *Clear All*.
  A bookmark stays on the same text when lines are added or removed above it,
  and it is removed when its document is closed. If its own line is deleted it moves to the line that took its place.
* **Indent**: a selection that ends exactly at the start of a line does not include that line; blank lines are skipped. One undo reverts the whole block.
* **Selection**: drag with the mouse, Shift-click, double-click a word, or Shift + arrow keys (also Shift-Home, Ctrl-Left/Right for words).
* **Movement**: arrows, ClrHome (line start), Ctrl-Home (top), Alt-Up/Down (page).
* Files are read as CR/LF or LF and saved with CR/LF; tabs display at 4 columns; Return auto-indents.

## Notes

* Shortcuts use the ASCII code the keyboard layout delivers, so Ctrl-Z/Y follow the key caps on German etc. layouts.
* Menu hints: `^` = Ctrl, `S^` = Shift-Ctrl.
* Shift+function keys are accepted both as scan codes 0x54..0x5D (real ST keyboard) and as F1..F10 plus the Shift bit (some emulators).
* On a 40-column (low resolution) screen the Bookmarks menu title is shortened to "Marks".
* Max 8 documents (AES window limit on plain TOS).
