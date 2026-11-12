# GEM Text Editor

Multi-document text editor for the Atari ST, written in C against GEM AES + VDI.
Menus and dialogs are built in code; there is no `.RSC` file.
See `README.md` for the user-facing key and menu reference.

## Build

    make                                   # -> build/gemedit.prg
    make TOOLKIT=/path/to/m68k-atari-elf   # default /usr/local/bin/m68k-atari-elf
    make clean

- Primary toolchain: **m68k-atari-elf** (`m68k-atari-elf-gcc`, link `-ltos -lvdi -laes`,
  convert with `m68k-atari-elf-prg`). Target CPU is 68000.
- `gemcompat.h` is the only place that knows about toolchains:
  `__M68K_ATARI_ELF__` (aes.h / vdi.h / tos.h), `__PUREC__`, otherwise
  m68k-atari-mint-gcc + gemlib (gem.h / osbind.h). Keep the other branches compiling.
- `-DNO_FSEL_EXINPUT` is set in the Makefile, so `fsel_input()` is used.
- New `.c` files must be added to `SOURCES` and to the header dependency lines
  at the bottom of the Makefile.

## Architecture

Dependencies point downwards only; the lower layer has no GEM calls.

| Layer | Files | Notes |
|-------|-------|-------|
| Entry / events | `main.c` | `evnt_multi` loop, key and AES message dispatch |
| Actions | `commands.c/h` | every user action is a `CMD_xxx` run by `cmd_execute()` |
| UI | `menu.c/h`, `dialog.c/h`, `window.c/h`, `app.c/h` | GEM only |
| Model (no GEM) | `doc.c/h`, `bookmark.c/h`, `textbuf.c/h`, `undo.c/h`, `clip.c/h`, `session.c/h` | portable C |

- **Adding a feature**: add a `CMD_xxx` in `commands.h`, implement it in
  `commands.c`, add a menu entry in the tables in `menu.c`, and a key in
  `main.c` (`ctrl_cmd` / `ctrl_shift_cmd` / `handle_key`). Menu and keyboard
  must always reach the same command.
- **Text storage**: gap buffer (`textbuf.c`) with a line-start table that is
  updated incrementally on every insert/delete. Text is stored with `\n`;
  files are read as LF or CR/LF and written as CR/LF.
- **Editing**: all changes go through `edit_insert` / `edit_delete` in `doc.c`
  (they record undo). `raw_insert` / `raw_delete` are for undo/redo replay only.
  Both pairs call the bookmark hooks and mark dirty lines.
- **Undo**: `UndoRec` records with a `chain` flag; chained records form one
  user step. Typing and backspace runs are coalesced. Cursor movement sets
  `no_coalesce`.
- **Bookmarks**: ten global slots holding (document, offset of a line start).
  `bm_after_insert` / `bm_after_delete` keep them on their text;
  `doc_replace_all` remaps them itself. `app_doc_remove` drops a closed
  document's bookmarks.
- **Redraw**: the model marks `dirty_lo..dirty_hi`; `win_refresh(d)` must be
  called after any edit or cursor change. It scrolls to the cursor, redraws
  dirty lines through the AES rectangle list, and updates sliders and title.

## Conventions

- C89 style: declarations at the top of a block, `/* */` comments, no VLAs,
  no `//`. 4-space indent, K&R braces for functions on their own line.
- Use `gint` (16-bit) for every value passed to or received from AES/VDI.
  Use `long` for text offsets and line numbers; plain `int` may be 16 or 32 bit
  depending on the toolchain.
- Access `OBJECT.ob_spec` only through the `OB_SPEC_*` macros in `gemcompat.h`.
- Check `malloc`/`realloc` results. The model reports out-of-memory by setting
  `d->error`; the UI shows it with `cmd_check_error(d)`.
- Fixed buffers: paths are `PATHLEN` (128), dialog fields `DLG_FLD` (30).
  Use bounded copies; the build should stay free of warnings with `-Wall`.
- Strings passed to `form_alert` must not contain `[`, `]` or `|` from user
  data; use `dlg_alert()` which sanitises them.

## GEM pitfalls already hit

- **Menu tree order matters.** The AES finds a title's drop-down by object
  *index*: root, bar, active, all titles, then the screen object, then the
  drop-down boxes in title order. Changing the creation order in
  `menu_build()` shifts every menu.
- Menu item text must be padded to the full drop-down width (`item_w`).
- Dynamic lists (Window, Bookmarks) are done by hiding items with `HIDETREE`
  and adjusting `ob_y` / box `ob_height` in `menu_sync()`.
- Shift+F1..F10 arrive as scan codes 0x54..0x5D on real hardware, but some
  emulators send 0x3B..0x44 plus the shift bit; `fkey_slot()` accepts both.
- Hatari's default (symbolic) keymap sends End as ST Undo (0x61) and
  Page Up/Down as keypad `(` `)` (0x63/0x64); `nav_key()` in `main.c` maps
  those and the PC codes (0x4F/0x49/0x51) before the Ctrl+letter dispatch.
- **Column selection** (`d->colsel`): per-line caret offsets in `cs_off`.
  Any edit not made by a column operation ends it (`raw_insert`/`raw_delete`
  check `cs_busy`), and so do `doc_set_cursor` / `doc_select`.
- Ctrl shortcuts test the ASCII code (1..26), not the scan code, so they
  follow national keyboard layouts.
- At most 8 windows on plain TOS (`MAX_DOCS`).
- Wrap drawing in `wind_update(BEG_UPDATE)` and hide the mouse; always clip to
  the window rectangle list.

## Testing

- No test suite is checked in. The model layer builds on any host:

      gcc -std=gnu89 -fsanitize=address,undefined your_test.c \
          doc.c textbuf.c undo.c clip.c bookmark.c

  Useful invariants: the line table matches a recount of `\n`; every bookmark
  offset is a line start within the buffer; undo-all followed by redo-all
  restores the text.
- GEM code can only be verified on an ST or in an emulator (Hatari). State
  clearly when a change to `menu.c`, `dialog.c`, `window.c`, `app.c` or
  `main.c` has not been run there.
