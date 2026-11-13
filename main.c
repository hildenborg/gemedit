/* main.c - start-up, event loop, keyboard and message dispatch */
#include "gemcompat.h"
#include "app.h"
#include "commands.h"
#include "menu.h"
#include "window.h"

#define SC_ESC    0x01
#define SC_BS     0x0E
#define SC_TAB    0x0F
#define SC_RET    0x1C
#define SC_ENTER  0x72
#define SC_UP     0x48
#define SC_DOWN   0x50
#define SC_LEFT   0x4B
#define SC_RIGHT  0x4D
#define SC_HOME   0x47
#define SC_DEL    0x53
#define SC_CLEFT  0x73          /* Ctrl+Left  */
#define SC_CRIGHT 0x74          /* Ctrl+Right */
#define SC_CEND   0x75          /* Ctrl+End   */
#define SC_CHOME  0x77          /* Ctrl+Home  */

/* The ST has no End / Page Up / Page Down keys.  Hatari (symbolic keymap,
 * the default) sends End as the ST Undo key and Page Up / Page Down as the
 * keypad ( and ) keys; PC keyboard interfaces (ARAnyM, Eiffel, Milan) send
 * the PC scan codes.  Both are accepted. */
#define SC_END    0x4F
#define SC_PGUP   0x49
#define SC_PGDN   0x51
#define SC_UNDO   0x61          /* ST Undo   = Hatari End       */
#define SC_KPLPAR 0x63          /* keypad (  = Hatari Page Up   */
#define SC_KPRPAR 0x64          /* keypad )  = Hatari Page Down */

/* Set stack and memory limits for the application.
 * There needs to be some memory left for GEM to allocate outside the program space. */
#ifdef __PUREC__
#else
#ifdef __M68K_ATARI_ELF__
int _stksize = 3;
#else
long _stksize = 3;
#endif
#endif

/* Ctrl+letter -> command. We test the ASCII value (1..26) the keyboard
 * layout delivers, so shortcuts follow the key caps on any national layout. */
static int ctrl_cmd(int letter)
{
    switch (letter) {
    case 'n': return CMD_NEW;
    case 'o': return CMD_OPEN;
    case 's': return CMD_SAVE;
    case 'q': return CMD_QUIT;
    case 'z': return CMD_UNDO;
    case 'y': return CMD_REDO;
    case 'x': return CMD_CUT;
    case 'c': return CMD_COPY;
    case 'v': return CMD_PASTE;
    case 'a': return CMD_SELALL;
    case 'f': return CMD_FIND;
    case 'g': return CMD_FINDNEXT;
    case 'h': return CMD_REPLACE;
    case 'w': return CMD_NEXTWIN;
    }
    return CMD_NONE;
}

/* Shift+Ctrl+letter commands (checked before the plain Ctrl ones). */
static int ctrl_shift_cmd(int letter)
{
    switch (letter) {
    case 's': return CMD_SAVEALL;
    case 'f': return CMD_FINDALL;
    case 'h': return CMD_REPLFILES;
    case 'o': return CMD_SESLOAD;
    case 'w': return CMD_SESSAVE;
    }
    return CMD_NONE;
}

/* F1..F10 give scan codes 0x3B..0x44, Shift+F1..F10 give 0x54..0x5D on the ST
 * keyboard; some emulators send the plain code plus the shift bit instead,
 * so both forms are accepted.  Returns slot 0..9 or -1. */
static int fkey_slot(int scan, int ks, int *set)
{
    if (scan >= 0x3B && scan <= 0x44) {
        *set = (ks & KS_SHIFT) != 0;
        return scan - 0x3B;
    }
    if (scan >= 0x54 && scan <= 0x5D) {
        *set = 1;
        return scan - 0x54;
    }
    return -1;
}

/* Cursor keys that are recognised by scan code alone, whatever the
 * Ctrl state and ASCII value.  Returns 1 if the key was handled. */
static int nav_key(Doc *d, int scan, gint ks)
{
    int ext = (ks & KS_SHIFT) != 0, ctrl = (ks & KS_CTRL) != 0;
    long dl = 0, dc = 0;

    if (ctrl && !ext && !(ks & KS_ALT)) {       /* Ctrl+arrow: column selection */
        switch (scan) {
        case SC_UP:     dl = -1; break;
        case SC_DOWN:   dl = 1;  break;
        case SC_LEFT:
        case SC_CLEFT:  dc = -1; break;
        case SC_RIGHT:
        case SC_CRIGHT: dc = 1;  break;
        }
        if (dl || dc) {
            doc_col_extend(d, dl, dc);
            win_refresh(d);
            return 1;
        }
    }

    switch (scan) {
    case SC_UP:
    case SC_DOWN:
        if (!(ks & KS_ALT))
            return 0;
        win_page(d, scan == SC_DOWN, 0);        /* Alt+Up/Down = page */
        return 1;
    case SC_PGUP:
    case SC_KPLPAR:
        win_page(d, 0, ext);
        return 1;
    case SC_PGDN:
    case SC_KPRPAR:
        win_page(d, 1, ext);
        return 1;
    case SC_HOME:   doc_move(d, ctrl ? MV_TOP : MV_HOME, ext);    break;
    case SC_CHOME:  doc_move(d, MV_TOP, ext);                     break;
    case SC_END:
    case SC_UNDO:   doc_move(d, ctrl ? MV_BOTTOM : MV_END, ext);  break;
    case SC_CEND:   doc_move(d, MV_BOTTOM, ext);                  break;
    case SC_ESC:
        if (!d->colsel)
            return 0;
        doc_col_off(d);                         /* Esc: back to one caret */
        break;
    default:
        return 0;
    }
    win_refresh(d);
    return 1;
}

static void handle_key(gint key, gint ks)
{
    unsigned k = (unsigned)key & 0xFFFF;
    int scan = (int)((k >> 8) & 0xFF), asc = (int)(k & 0xFF), cmd;
    int ext = (ks & KS_SHIFT) != 0, slot, set = 0;
    Doc *d = app_top_doc();

    if (d && nav_key(d, scan, ks))
        return;

    if ((ks & KS_CTRL) && asc >= 1 && asc <= 26 &&
        scan != SC_BS && scan != SC_TAB && scan != SC_RET && scan != SC_ENTER) {
        if (asc == 5) {                         /* Ctrl+E = end of line */
            if (d) {
                doc_move(d, MV_END, ext);
                win_refresh(d);
            }
            return;
        }
        cmd = ext ? ctrl_shift_cmd('a' + asc - 1) : CMD_NONE;
        if (!cmd)
            cmd = ctrl_cmd('a' + asc - 1);
        if (cmd)
            cmd_execute(cmd);
        return;
    }
    slot = fkey_slot(scan, ks, &set);
    if (slot >= 0) {                            /* bookmarks */
        cmd_execute((set ? CMD_BMSET0 : CMD_BMGO0) + slot);
        return;
    }

    if (!d)
        return;

    switch (scan) {
    case SC_UP:     doc_move(d, MV_UP, ext);     break;
    case SC_DOWN:   doc_move(d, MV_DOWN, ext);   break;
    case SC_LEFT:   doc_move(d, (ks & KS_CTRL) ? MV_WORDL : MV_LEFT, ext);  break;
    case SC_RIGHT:  doc_move(d, (ks & KS_CTRL) ? MV_WORDR : MV_RIGHT, ext); break;
    case SC_CLEFT:  doc_move(d, MV_WORDL, ext);  break;
    case SC_CRIGHT: doc_move(d, MV_WORDR, ext);  break;
    case SC_DEL:    doc_delete(d);    break;
    case SC_BS:     doc_backspace(d); break;
    case SC_RET:
    case SC_ENTER:  doc_newline(d);   break;
    case SC_TAB:
        if (ks & KS_SHIFT)
            doc_unindent(d);                    /* Shift-Tab: remove one indent */
        else if (doc_has_sel(d))
            doc_indent(d);                      /* Tab on a selection: indent   */
        else
            doc_type(d, '\t');
        break;
    default:
        if (asc >= 32 && asc != 127 && !(ks & KS_CTRL))
            doc_type(d, asc);
        else
            return;
        break;
    }
    win_refresh(d);
    cmd_check_error(d);
}

static void handle_button(gint mx, gint my, gint ks, gint clicks)
{
    gint wh = wind_find(mx, my);
    Doc *d = app_doc_from_handle(wh);
    if (d && d == app_top_doc()) {
        win_click(d, mx, my, ks, clicks);
        cmd_check_error(d);
    }
}

static void handle_message(gint *m)
{
    Doc *d;
    Rect r;

    switch (m[0]) {
    case MN_SELECTED:
        menu_tnormal(app.menu, m[3], 1);
        cmd_execute(menu_cmd_for_obj(m[4]));
        break;
    case WM_REDRAW:
        d = app_doc_from_handle(m[3]);
        if (d) {
            r.x = m[4]; r.y = m[5]; r.w = m[6]; r.h = m[7];
            win_redraw(d, &r);
        }
        break;
    case WM_TOPPED:
    case WM_NEWTOP:
        d = app_doc_from_handle(m[3]);
        if (d) win_set_top(d);
        break;
    case WM_CLOSED:
        d = app_doc_from_handle(m[3]);
        if (d) cmd_close_doc(d);
        break;
    case WM_FULLED:
        d = app_doc_from_handle(m[3]);
        if (d) win_fulled(d);
        break;
    case WM_ARROWED:
        d = app_doc_from_handle(m[3]);
        if (d) win_arrowed(d, m[4]);
        break;
    case WM_VSLID:
        d = app_doc_from_handle(m[3]);
        if (d) win_vslid(d, m[4]);
        break;
    case WM_HSLID:
        d = app_doc_from_handle(m[3]);
        if (d) win_hslid(d, m[4]);
        break;
    case WM_SIZED:
    case WM_MOVED:
        d = app_doc_from_handle(m[3]);
        if (d) win_set_rect(d, m[4], m[5], m[6], m[7]);
        break;
    case AP_TERM:
        cmd_execute(CMD_QUIT);
        break;
    }
}

static void event_loop(void)
{
    gint msg[8], mx, my, mb, ks, key, clicks, ev;

    while (app.running) {
        ev = evnt_multi(MU_KEYBD | MU_BUTTON | MU_MESAG,
                        2, 1, 1,                    /* up to 2 clicks, left button down */
                        0, 0, 0, 0, 0,
                        0, 0, 0, 0, 0,
                        msg, 0, 0,
                        &mx, &my, &mb, &ks, &key, &clicks);
        if (ev & MU_MESAG)
            handle_message(msg);
        if ((ev & MU_KEYBD) && app.running)
            handle_key(key, ks);
        if ((ev & MU_BUTTON) && app.running)
            handle_button(mx, my, ks, clicks);
        if (app.running)
            menu_sync();
    }
}

int main(void)
{
    if (app_init())
        return 1;
    menu_build();
    app_desk_update();
    cmd_execute(CMD_NEW);
    event_loop();
    app_exit();
    return 0;
}
