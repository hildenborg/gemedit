/* menu.c - builds the GEM menu tree programmatically */
#include "gemcompat.h"
#include "app.h"
#include "menu.h"
#include "commands.h"
#include "clip.h"
#include "bookmark.h"

#define MAXMOBJ  110
#define ITEMW    24                     /* characters per drop-down line        */
#define BMW      30                     /* ... in the Bookmarks drop-down       */
#define TXTW     (BMW + 4)
#define SOLID_WHITE_BOX   0x00FF1170L   /* 1px outline, solid white */
#define SOLID_WHITE_BAR   0x00001170L

typedef struct { int cmd; const char *label; const char *key; } MItem;

static OBJECT tree[MAXMOBJ];
static char   text[MAXMOBJ][TXTW];
static int    obj_cmd[MAXMOBJ];
static int    cmd_obj[CMD_COUNT];
static int    nobj;
static int    item_w = ITEMW;           /* width of the drop-down being built   */
static signed char enab[CMD_COUNT];
static signed char chk[MAX_DOCS];
static int    win_box, win_base_items, win_ndocs = -1;
static int    bm_box, bm_base_h;

static const MItem m_file[] = {
    { CMD_NEW,    "New",         "^N" },
    { CMD_OPEN,   "Open...",     "^O" },
    { CMD_CLOSE,  "Close",       ""   },
    { CMD_SEP, 0, 0 },
    { CMD_SAVE,   "Save",        "^S" },
    { CMD_SAVEAS, "Save As...",  ""   },
    { CMD_SAVEALL,"Save All",    "S^S" },
    { CMD_SEP, 0, 0 },
    { CMD_SESLOAD,"Load Session...", "S^O" },
    { CMD_SESSAVE,"Save Session...", "S^W" },
    { CMD_SEP, 0, 0 },
    { CMD_QUIT,   "Quit",        "^Q" }
};
static const MItem m_edit[] = {
    { CMD_UNDO,   "Undo",        "^Z" },
    { CMD_REDO,   "Redo",        "^Y" },
    { CMD_SEP, 0, 0 },
    { CMD_CUT,    "Cut",         "^X" },
    { CMD_COPY,   "Copy",        "^C" },
    { CMD_PASTE,  "Paste",       "^V" },
    { CMD_SEP, 0, 0 },
    { CMD_SELALL, "Select All",  "^A" }
};
static const MItem m_search[] = {
    { CMD_FIND,     "Find...",           "^F" },
    { CMD_FINDNEXT, "Find Next",         "^G" },
    { CMD_REPLACE,  "Replace...",        "^H" },
    { CMD_SEP, 0, 0 },
    { CMD_FINDALL,  "Find in Files...",  "S^F" },
    { CMD_REPLFILES,"Replace in Files...","S^H" }
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void pad(char *s, int w)
{
    int n = (int)strlen(s);
    while (n < w)
        s[n++] = ' ';
    s[n] = 0;
}

static int new_obj(int type, int flags, int state, long spec, int x, int y, int w, int h)
{
    OBJECT *o = &tree[nobj];
    o->ob_next = o->ob_head = o->ob_tail = -1;
    o->ob_type = type;
    o->ob_flags = flags;
    o->ob_state = state;
    OB_SPEC_LONG(o, spec);
    o->ob_x = x;
    o->ob_y = y;
    o->ob_width = w;
    o->ob_height = h;
    obj_cmd[nobj] = CMD_NONE;
    return nobj++;
}

static int add_item(int box, int cmd, const char *label, const char *key, int *y)
{
    int o = new_obj(G_STRING, NONE, NORMAL, 0L, 0, *y, item_w * app.cw, app.ch);
    if (cmd == CMD_SEP) {
        memset(text[o], '-', item_w);
        text[o][item_w] = 0;
        tree[o].ob_state = DISABLED;
    } else if (label) {
        sprintf(text[o], "  %-16.16s%-4.4s  ", label, key);
        pad(text[o], item_w);
    } else {
        memset(text[o], ' ', item_w);
        text[o][item_w] = 0;
    }
    OB_SPEC_STR(&tree[o], text[o]);
    objc_add(tree, box, o);
    obj_cmd[o] = cmd > 0 ? cmd : CMD_NONE;
    if (cmd > 0)
        cmd_obj[cmd] = o;
    *y += app.ch;
    return o;
}

static int add_title(int active, const char *s, int *x)
{
    int len = (int)strlen(s), o;
    o = new_obj(G_TITLE, NONE, NORMAL, 0L, *x, 0, len * app.cw, app.ch + 3);
    strcpy(text[o], s);
    OB_SPEC_STR(&tree[o], text[o]);
    objc_add(tree, active, o);
    *x += len * app.cw;
    return o;
}

static int add_box(int screen, int x)
{
    int bw = item_w * app.cw, b;
    if (x + bw + 2 > app.scrw)
        x = app.scrw - bw - 2;
    /* Keep the 1 pixel outline on screen: TOS 2.06 does not clip the area it
     * saves under a drop-down, and a box at x = 0 (outline at -1) makes it
     * read past the end of screen memory (bus error on the Desk menu). */
    if (x < 1)
        x = 1;
    b = new_obj(G_BOX, NONE, NORMAL, SOLID_WHITE_BOX, x, 0, bw, app.ch);
    objc_add(tree, screen, b);
    return b;
}

static void fill_menu(int box, const MItem *it, int n)
{
    int i, y = 0;
    for (i = 0; i < n; i++)
        add_item(box, it[i].cmd, it[i].label, it[i].key, &y);
    tree[box].ob_height = y;
}

void menu_build(void)
{
    int barh = app.ch + 3, root, bar, active, screen, box, i, x = 0, y;
    const char *titles[6];
    int tx[6];

    titles[0] = " Desk ";
    titles[1] = " File ";
    titles[2] = " Edit ";
    titles[3] = " Search ";
    titles[4] = (app.scrw / app.cw >= 48) ? " Bookmarks " : " Marks ";
    titles[5] = " Window ";

    memset(enab, -1, sizeof enab);
    memset(chk, -1, sizeof chk);
    memset(cmd_obj, 0, sizeof cmd_obj);
    nobj = 0;
    item_w = ITEMW;

    root   = new_obj(G_IBOX, NONE, NORMAL, 0L, 0, 0, app.scrw, app.scrh);
    bar    = new_obj(G_BOX,  NONE, NORMAL, SOLID_WHITE_BAR, 0, 0, app.scrw, barh);
    active = new_obj(G_IBOX, NONE, NORMAL, 0L, 0, 0, 0, barh);
    objc_add(tree, root, bar);
    objc_add(tree, bar, active);

    /* The AES locates a title's drop-down by object INDEX, so the titles
     * must directly follow 'active'; the screen object and the drop-down
     * boxes come after them, in the same order as the titles. */
    for (i = 0; i < 6; i++) {
        tx[i] = x;
        add_title(active, titles[i], &x);
    }
    tree[active].ob_width = x;

    screen = new_obj(G_IBOX, NONE, NORMAL, 0L, 0, barh, app.scrw, app.scrh - barh);
    objc_add(tree, root, screen);

    /* Desk: About + room for 6 desk accessories */
    box = add_box(screen, tx[0]);
    y = 0;
    add_item(box, CMD_ABOUT, "About...", "", &y);
    add_item(box, CMD_SEP, 0, 0, &y);
    for (i = 0; i < 6; i++)
        tree[add_item(box, CMD_NONE, 0, 0, &y)].ob_state = DISABLED;
    tree[box].ob_height = y;

    box = add_box(screen, tx[1]);
    fill_menu(box, m_file, COUNT(m_file));
    box = add_box(screen, tx[2]);
    fill_menu(box, m_edit, COUNT(m_edit));
    box = add_box(screen, tx[3]);
    fill_menu(box, m_search, COUNT(m_search));

    /* Bookmarks: Clear All, separator, then one slot per bookmark.  Only the
     * set bookmarks are shown; menu_sync() places them one below the other. */
    item_w = BMW;
    box = add_box(screen, tx[4]);
    bm_box = box;
    y = 0;
    add_item(box, CMD_BMCLEAR, "Clear All", "", &y);
    add_item(box, CMD_SEP, 0, 0, &y);
    bm_base_h = y;
    for (i = 0; i < BM_MAX; i++)
        add_item(box, CMD_BMGO0 + i, "", "", &y);
    tree[box].ob_height = bm_base_h;
    item_w = ITEMW;

    /* Window: Next Window, separator, one slot per open document */
    box = add_box(screen, tx[5]);
    win_box = box;
    y = 0;
    add_item(box, CMD_NEXTWIN, "Next Window", "^W", &y);
    add_item(box, CMD_SEP, 0, 0, &y);
    win_base_items = 2;
    for (i = 0; i < MAX_DOCS; i++)
        add_item(box, CMD_WIN0 + i, "", "", &y);
    tree[box].ob_height = win_base_items * app.ch;

    tree[nobj - 1].ob_flags |= LASTOB;      /* Window box is built last */
    app.menu = tree;
    menu_bar(tree, 1);
    win_ndocs = -1;
    menu_sync();
}

int menu_cmd_for_obj(int obj)
{
    if (obj < 0 || obj >= nobj)
        return CMD_NONE;
    return obj_cmd[obj];
}

static void set_enable(int cmd, int on)
{
    on = on ? 1 : 0;
    if (enab[cmd] != on) {
        enab[cmd] = (signed char)on;
        menu_ienable(tree, cmd_obj[cmd], on);
    }
}

static void sync_bookmarks(void)
{
    int i, o, y = bm_base_h, any = 0;
    long ln;

    for (i = 0; i < BM_MAX; i++) {
        o = cmd_obj[CMD_BMGO0 + i];
        if (bm_tab[i].doc) {
            ln = bm_line(i) + 1;
            if (ln > 99999L)
                ln = 99999L;
            sprintf(text[o], "  F%-2d %-14.14s Ln %-5ld ", i + 1,
                    bm_tab[i].doc->name, ln);
            tree[o].ob_flags &= ~HIDETREE;
            tree[o].ob_y = y;
            y += app.ch;
            any = 1;
        } else
            tree[o].ob_flags |= HIDETREE;
    }
    tree[bm_box].ob_height = y;
    set_enable(CMD_BMCLEAR, any);
}

void menu_sync(void)
{
    Doc *top = app_top_doc();
    int has = top != 0, i, o, anymod = 0;

    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i]->modified)
            anymod = 1;

    set_enable(CMD_CLOSE,    has);
    set_enable(CMD_SAVE,     has);
    set_enable(CMD_SAVEAS,   has);
    set_enable(CMD_SAVEALL,  anymod);
    set_enable(CMD_UNDO,     has && undo_can_undo(&top->undo));
    set_enable(CMD_REDO,     has && undo_can_redo(&top->undo));
    set_enable(CMD_SESSAVE,  app.ndocs > 0);
    set_enable(CMD_CUT,      has && doc_has_sel(top));
    set_enable(CMD_COPY,     has && doc_has_sel(top));
    set_enable(CMD_PASTE,    has && clip_len() > 0);
    set_enable(CMD_SELALL,   has);
    set_enable(CMD_FIND,     has);
    set_enable(CMD_FINDNEXT, has);
    set_enable(CMD_REPLACE,  has);
    set_enable(CMD_FINDALL,  app.ndocs > 0);
    set_enable(CMD_REPLFILES, app.ndocs > 0);
    set_enable(CMD_NEXTWIN,  app.ndocs > 1);

    sync_bookmarks();

    /* Window list: names change, box height follows number of documents */
    for (i = 0; i < MAX_DOCS; i++) {
        o = cmd_obj[CMD_WIN0 + i];
        if (i < app.ndocs) {
            sprintf(text[o], "  %d %-18.18s  ", i + 1, app.docs[i]->name);
            tree[o].ob_flags &= ~HIDETREE;
            if (chk[i] != (app.docs[i] == top)) {
                chk[i] = (signed char)(app.docs[i] == top);
                menu_icheck(tree, o, chk[i]);
            }
        } else {
            tree[o].ob_flags |= HIDETREE;
            chk[i] = -1;
        }
    }
    if (win_ndocs != app.ndocs) {
        win_ndocs = app.ndocs;
        tree[win_box].ob_height = (win_base_items + app.ndocs) * app.ch;
    }
}
