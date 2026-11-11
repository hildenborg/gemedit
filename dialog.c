/* dialog.c - GEM dialogs built in code (no resource file) */
#include "gemcompat.h"
#include "app.h"
#include "dialog.h"

/* ------------------------------------------------------------------ */
/* alerts                                                              */
/* ------------------------------------------------------------------ */

static void clean(char *dst, const char *src, int max)
{
    int n = 0;
    for (; *src && n < max; src++, n++)
        dst[n] = (*src == '|' || *src == '[' || *src == ']') ? '_' : *src;
    dst[n] = 0;
}

void dlg_alert(int icon, const char *l1, const char *l2)
{
    char a[40], b[40], s[160];
    clean(a, l1, 30);
    clean(b, l2, 30);
    sprintf(s, "[%d][%s|%s][OK]", icon, a, b);
    form_alert(1, s);
}

int dlg_ask_save(const char *name)
{
    char n[40], s[160];
    clean(n, name, 28);
    sprintf(s, "[2][Save changes to|%s ?][Save|Ignore|Cancel]", n);
    return form_alert(1, s);
}

int dlg_confirm_overwrite(const char *name)
{
    char n[40], s[160];
    clean(n, name, 28);
    sprintf(s, "[2][%s|already exists.|Overwrite it?][Overwrite|Cancel]", n);
    return form_alert(2, s) == 1;
}

/* ------------------------------------------------------------------ */
/* file selector                                                       */
/* ------------------------------------------------------------------ */

#define FS_PATHLEN 256                  /* room for "X:" + 127 char dir + "\\*.*" */

static char fs_path[FS_PATHLEN];
static int  fs_ready = 0;

static void fs_init(void)
{
    char dir[PATHLEN];
    int n;
    dir[0] = 0;
    Dgetpath(dir, 0);
    dir[PATHLEN - 1] = 0;
    n = (int)strlen(dir);
    sprintf(fs_path, "%c:%s%s*.*", 'A' + Dgetdrv(), dir,
            (n > 0 && dir[n - 1] == '\\') ? "" : "\\");
    fs_ready = 1;
}

int dlg_fileselect(const char *title, char *out, const char *defname)
{
    static char fname[64];
    gint btn = 0;
    char *p;
    size_t dl;

    if (!fs_ready)
        fs_init();
    strncpy(fname, defname, 63);
    fname[63] = 0;
#ifdef HAVE_FSEL_EXINPUT
    fsel_exinput(fs_path, fname, &btn, (char *)title);
#else
    (void)title;
    fsel_input(fs_path, fname, &btn);
#endif
    if (!btn || !fname[0])
        return 0;

    p = fs_path + strlen(fs_path);              /* directory part of the mask */
    while (p > fs_path && p[-1] != '\\' && p[-1] != '/' && p[-1] != ':')
        p--;
    dl = (size_t)(p - fs_path);
    if (dl + strlen(fname) >= PATHLEN)          /* caller's buffer is PATHLEN */
        return 0;
    memcpy(out, fs_path, dl);
    strcpy(out + dl, fname);
    return 1;
}

/* ------------------------------------------------------------------ */
/* find / replace dialog                                               */
/* ------------------------------------------------------------------ */

static OBJECT dt[16];
static int    dn;
static TEDINFO ted[2];
static char   tbuf[2][DLG_FLD + 2], tmpl[2][DLG_FLD + 2], valid[2][DLG_FLD + 2];

static int dobj(int parent, int type, int flags, int state, int x, int y, int w, int h)
{
    OBJECT *o = &dt[dn];
    o->ob_next = o->ob_head = o->ob_tail = -1;
    o->ob_type = type;
    o->ob_flags = flags;
    o->ob_state = state;
    o->ob_x = x;
    o->ob_y = y;
    o->ob_width = w;
    o->ob_height = h;
    OB_SPEC_LONG(o, 0L);
    if (parent >= 0)
        objc_add(dt, parent, dn);
    return dn++;
}

static int dstr(int parent, int type, int flags, const char *s, int x, int y, int w, int h)
{
    int o = dobj(parent, type, flags, NORMAL, x, y, w, h);
    OB_SPEC_STR(&dt[o], (char *)s);
    return o;
}

static int dfield(int parent, int i, int x, int y, const char *init)
{
    int o = dobj(parent, G_FBOXTEXT, EDITABLE, NORMAL, x, y, DLG_FLD * app.cw + 2, app.ch + 4);
    memset(tmpl[i], '_', DLG_FLD);
    tmpl[i][DLG_FLD] = 0;
    memset(valid[i], 'X', DLG_FLD);
    valid[i][DLG_FLD] = 0;
    strncpy(tbuf[i], init, DLG_FLD);
    tbuf[i][DLG_FLD] = 0;
    memset(&ted[i], 0, sizeof ted[i]);
    ted[i].te_ptext = tbuf[i];
    ted[i].te_ptmplt = tmpl[i];
    ted[i].te_pvalid = valid[i];
    ted[i].te_font = 3;                 /* IBM (system) font */
    ted[i].te_just = 0;
    ted[i].te_color = 0x1180;
    ted[i].te_thickness = 1;
    ted[i].te_txtlen = DLG_FLD + 1;
    ted[i].te_tmplen = DLG_FLD + 1;
    OB_SPEC_TED(&dt[o], &ted[i]);
    return o;
}

static int run_dialog(OBJECT *t, int edit)
{
    gint x, y, w, h, ex;
    form_center(t, &x, &y, &w, &h);
    wind_update(BEG_UPDATE);
    wind_update(BEG_MCTRL);
    form_dial(FMD_START, 0, 0, 0, 0, x, y, w, h);
    objc_draw(t, ROOT, MAX_DEPTH, x, y, w, h);
    graf_mouse(ARROW, 0);
    ex = form_do(t, edit);
    form_dial(FMD_FINISH, 0, 0, 0, 0, x, y, w, h);
    wind_update(END_MCTRL);
    wind_update(END_UPDATE);
    return ex & 0x7FFF;
}

int dlg_search(const char *title, int mode, char *find, char *repl,
               int *match_case, int *all)
{
    int cw = app.cw, ch = app.ch;
    int root, f1, chk, allchk = -1, i, x, nb, ex, row, bw;
    int bobj[4], bcode[4];
    const char *blab[4];
    int replace = mode >= 1;

    dn = 0;
    row = replace ? 7 : 5;
    root = dobj(-1, G_BOX, NONE, OUTLINED, 0, 0, 50 * cw,
                (row + (mode == 1 ? 6 : 4)) * ch);
    OB_SPEC_LONG(&dt[root], 0x00021170L);

    dstr(root, G_STRING, NONE, title, 2 * cw, ch, 46 * cw, ch);
    dstr(root, G_STRING, NONE, "Find:", 2 * cw, 3 * ch, 12 * cw, ch);
    f1 = dfield(root, 0, 16 * cw, 3 * ch - 2, find);
    if (replace) {
        dstr(root, G_STRING, NONE, "Replace with:", 2 * cw, 5 * ch, 13 * cw, ch);
        dfield(root, 1, 16 * cw, 5 * ch - 2, repl);
    }
    chk = dobj(root, G_BUTTON, SELECTABLE, *match_case ? SELECTED : NORMAL,
               16 * cw, row * ch - 2, 12 * cw, ch + 4);
    OB_SPEC_STR(&dt[chk], "Match case");
    if (mode == 1) {
        row += 2;
        allchk = dobj(root, G_BUTTON, SELECTABLE, *all ? SELECTED : NORMAL,
                      16 * cw, row * ch - 2, 25 * cw, ch + 4);
        OB_SPEC_STR(&dt[allchk], "Replace all occurrences");
    }

    if (mode == 0) {
        blab[0] = "Find";        bcode[0] = DLG_FIND;
        blab[1] = "Cancel";      bcode[1] = DLG_CANCEL;
        nb = 2;
    } else if (mode == 1) {
        blab[0] = "Replace";     bcode[0] = DLG_REPLACE;
        blab[1] = "Find next";   bcode[1] = DLG_FINDNEXT;
        blab[2] = "Cancel";      bcode[2] = DLG_CANCEL;
        nb = 3;
    } else {
        blab[0] = "Replace all"; bcode[0] = DLG_REPLACEALL;
        blab[1] = "Cancel";      bcode[1] = DLG_CANCEL;
        nb = 2;
    }
    x = 2 * cw;
    for (i = 0; i < nb; i++) {
        bw = ((int)strlen(blab[i]) + 2) * cw;
        bobj[i] = dstr(root, G_BUTTON, SELECTABLE | EXIT | (i == 0 ? DEFAULT : 0),
                       blab[i], x, (row + 2) * ch - 2, bw, ch + 4);
        x += bw + cw;
    }
    dt[dn - 1].ob_flags |= LASTOB;

    ex = run_dialog(dt, f1);

    for (i = 0; i < nb; i++)
        if (bobj[i] == ex) {
            if (bcode[i] != DLG_CANCEL) {
                strcpy(find, tbuf[0]);
                if (replace)
                    strcpy(repl, tbuf[1]);
                *match_case = (dt[chk].ob_state & SELECTED) != 0;
                *all = allchk >= 0 && (dt[allchk].ob_state & SELECTED) != 0;
            }
            return bcode[i];
        }
    return DLG_CANCEL;
}
