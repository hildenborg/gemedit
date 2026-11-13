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
| `session.c/h` | session file: list of open documents and bookmarks |
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
| Ctrl-Z / Ctrl-Y | undo / redo | Ctrl-H | find & replace |
| Ctrl-W | next window | **Shift-Ctrl-H** | **replace in all open files** |
| **Tab** | indent selected lines (inserts a tab otherwise) | **Shift-Tab** | remove one indent (selected lines or current line) |
| **F1..F10** | go to bookmark | **Shift+F1..F10** | set bookmark at the current line |
| Home / End | start / end of line | Ctrl-Home / Ctrl-End | start / end of document |
| Page Up / Page Down | one page up / down (also Alt-Up/Down) | Esc | end a column selection |
| **Shift-Ctrl-O** | **load session** | **Shift-Ctrl-W** | **save session** |
| **Ctrl + click / drag** | **column (box) selection** | **Ctrl + arrows** | **extend column selection** |

* **Replace dialog**: has a *Replace all occurrences* check box. Unchecked, *Replace* replaces the selected match and finds the next; checked, it replaces every match in the current file (one undo step).
* **Bookmarks menu**: lists the bookmarks that are set as `F<n> <document> Ln <line>` (selecting one jumps there) and *Clear All*.
  A bookmark stays on the same text when lines are added or removed above it,
  and it is removed when its document is closed. If its own line is deleted it moves to the line that took its place.
* **Indent**: a selection that ends exactly at the start of a line does not include that line; blank lines are skipped. One undo reverts the whole block.
* **Selection**: drag with the mouse, Shift-click, double-click a word, or Shift + arrow keys (also Shift-Home/End, Shift-Page Up/Down, Shift-Ctrl-Left/Right for words).
* **Movement**: arrows, Home / End, Ctrl-Home / Ctrl-End, Page Up / Page Down. Paging moves the text with the caret, so the caret keeps its row in the window.
* **End / Page keys**: the ST has none. Hatari (default symbolic keymap) sends End as the ST *Undo* key and Page Up / Down as the keypad *(* and *)* keys, so those ST keys act as End / Page Up / Page Down. The PC scan codes (ARAnyM, Eiffel, Milan) are accepted as well.
* **Column selection** (as in VS Code): Ctrl-click or Ctrl-drag selects a box from the caret (or current box corner) to the mouse. Each line in the box gets its own caret; lines that end left of the box get none. Typing, Backspace, Delete, Cut and Paste act on every caret (one undo step); Left/Right/Home/End move every caret within its line. Copy puts one line per caret on the clipboard; pasting text with as many lines as there are carets puts one line at each caret. Return or pasting other multi-line text inserts it at every caret and ends the column selection; so do Up/Down, a click or Esc. Tab with a selected box indents its lines.
* **Sessions**: *File > Save Session* writes the open documents (path and caret line) and the bookmarks to a text file. *Load Session* asks to save modified documents, closes all documents, opens the ones listed and restores the bookmarks and the top window. Untitled documents are not saved in a session.
* Files are read as CR/LF or LF and saved with CR/LF; tabs display at 4 columns; Return auto-indents.

## Notes

* Shortcuts use the ASCII code the keyboard layout delivers, so Ctrl-Z/Y follow the key caps on German etc. layouts.
* Menu hints: `^` = Ctrl, `S^` = Shift-Ctrl.
* Shift+function keys are accepted both as scan codes 0x54..0x5D (real ST keyboard) and as F1..F10 plus the Shift bit (some emulators).
* On a 40-column (low resolution) screen the Bookmarks menu title is shortened to "Marks".
* Max 8 documents (AES window limit on plain TOS).
