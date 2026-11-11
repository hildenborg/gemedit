/* window.c - GEM window handling for a document */
#include "gemcompat.h"
#include "app.h"
#include "window.h"

#define GADGETS (NAME | CLOSER | FULLER | MOVER | INFO | SIZER | UPARROW | \
                 DNARROW | VSLIDE | LFARROW | RTARROW | HSLIDE)
#define MIN_W   120
#define MIN_H   80
#define RAWMAX  2048
#define XBMAX   400

static char rawbuf[RAWMAX];
static char xbuf[XBMAX];

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static void wset_str(gint h, gint field, const char *s)
{
    long a = (long)s;
    wind_set(h, field, (gint)((a >> 16) & 0xFFFFL), (gint)(a & 0xFFFFL), 0, 0);
}

static void work_rect(const Doc *d, Rect *r)
{
    wind_get(d->whandle, WF_WORKXYWH, &r->x, &r->y, &r->w, &r->h);
}

static void to_pxy(const Rect *r, gint *p)
{
    p[0] = r->x;
    p[1] = r->y;
    p[2] = r->x + r->w - 1;
    p[3] = r->y + r->h - 1;
}

static long floor_div(long a, long b)
{
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */

static void fill_xor(gint x1, gint y1, gint x2, gint y2)
{
    gint p[4];
    p[0] = x1; p[1] = y1; p[2] = x2; p[3] = y2;
    vswr_mode(app.vh, MD_XOR);
    vsf_color(app.vh, 1);
    vr_recfl(app.vh, p);
    vswr_mode(app.vh, MD_REPLACE);
}

static void draw_line(Doc *d, const Rect *w, long line, long row, int has_sel,
                      long ss, long se)
{
    TextBuf *t = &d->tb;
    long ls = tb_line_start(t, line), le = tb_line_end(t, line);
    long ncopy = le - ls, col, i, nvis, last = -1, next, a, b, c;
    unsigned char ch;
    gint y = (gint)(w->y + row * app.ch);

    nvis = d->page_cols + 1;
    if (nvis > XBMAX - 1)
        nvis = XBMAX - 1;
    if (ncopy > d->left_col + nvis + 1)
        ncopy = d->left_col + nvis + 1;
    if (ncopy > RAWMAX)
        ncopy = RAWMAX;
    tb_copy(t, ls, ncopy, rawbuf);
    memset(xbuf, ' ', (size_t)nvis);

    col = 0;
    for (i = 0; i < ncopy; i++) {
        ch = (unsigned char)rawbuf[i];
        if (ch == '\t')
            next = col + TABW - (col % TABW);
        else {
            next = col + 1;
            if (col >= d->left_col && col - d->left_col < nvis) {
                xbuf[col - d->left_col] = (ch < 32) ? '.' : (char)ch;
                last = col - d->left_col;
            }
        }
        col = next;
        if (col - d->left_col >= nvis)
            break;
    }
    if (last >= 0) {
        xbuf[last + 1] = 0;
        vswr_mode(app.vh, MD_REPLACE);
        vst_color(app.vh, 1);
        v_gtext(app.vh, w->x, y, xbuf);
    }

    if (has_sel && se > ls && ss <= le) {
        a = (ss <= ls) ? 0 : doc_vcol(d, line, ss);
        b = (se > le) ? doc_vcol(d, line, le) + 1 : doc_vcol(d, line, se);
        a -= d->left_col;
        b -= d->left_col;
        if (a < 0) a = 0;
        if (b > d->page_cols + 1) b = d->page_cols + 1;
        if (b > a)
            fill_xor((gint)(w->x + a * app.cw), y,
                     (gint)(w->x + b * app.cw - 1), (gint)(y + app.ch - 1));
    }

    if (d->cursor >= ls && d->cursor <= le) {
        c = doc_vcol(d, line, d->cursor) - d->left_col;
        if (c >= 0 && c <= d->page_cols)
            fill_xor((gint)(w->x + c * app.cw), y,
                     (gint)(w->x + c * app.cw + 1), (gint)(y + app.ch - 1));
    }
}

static void draw_content(Doc *d, const Rect *c)
{
    Rect w;
    gint p[4];
    long r0, r1, row, line, ss = 0, se = 0;
    int has_sel;

    work_rect(d, &w);
    vswr_mode(app.vh, MD_REPLACE);
    vsf_interior(app.vh, FIS_SOLID);
    vsf_color(app.vh, 0);
    vsf_perimeter(app.vh, 0);
    to_pxy(c, p);
    vr_recfl(app.vh, p);

    has_sel = doc_sel_range(d, &ss, &se);
    r0 = (c->y - w.y) / app.ch;
    r1 = (c->y + c->h - 1 - w.y) / app.ch;
    for (row = r0; row <= r1; row++) {
        line = d->top_line + row;
        if (line >= tb_line_count(&d->tb))
            break;
        draw_line(d, &w, line, row, has_sel, ss, se);
    }
}

static void redraw_rect(Doc *d, const Rect *area)
{
    Rect w, t, c, a;
    gint p[4];

    if (d->whandle < 0)
        return;
    work_rect(d, &w);
    if (!rect_intersect(area, &w, &a))
        return;

    wind_update(BEG_UPDATE);
    graf_mouse(M_OFF, 0);
    wind_get(d->whandle, WF_FIRSTXYWH, &t.x, &t.y, &t.w, &t.h);
    while (t.w > 0 && t.h > 0) {
        if (rect_intersect(&t, &a, &c)) {
            to_pxy(&c, p);
            vs_clip(app.vh, 1, p);
            draw_content(d, &c);
        }
        wind_get(d->whandle, WF_NEXTXYWH, &t.x, &t.y, &t.w, &t.h);
    }
    p[0] = 0; p[1] = 0; p[2] = app.scrw - 1; p[3] = app.scrh - 1;
    vs_clip(app.vh, 0, p);
    graf_mouse(M_ON, 0);
    wind_update(END_UPDATE);
}

static void redraw_lines(Doc *d, long lo, long hi)
{
    Rect w, r;
    long r0, r1, maxrow;

    work_rect(d, &w);
    maxrow = (w.h + app.ch - 1) / app.ch - 1;
    r0 = lo - d->top_line;
    r1 = (hi >= LINE_ALL) ? maxrow : hi - d->top_line;
    if (r1 < 0 || r0 > maxrow)
        return;
    if (r0 < 0) r0 = 0;
    if (r1 > maxrow) r1 = maxrow;
    r.x = w.x;
    r.w = w.w;
    r.y = (gint)(w.y + r0 * app.ch);
    r.h = (gint)((r1 - r0 + 1) * app.ch);
    redraw_rect(d, &r);
}

static void redraw_all(Doc *d)
{
    Rect w;
    work_rect(d, &w);
    redraw_rect(d, &w);
}

static void flush_dirty(Doc *d)
{
    long lo = d->dirty_lo, hi = d->dirty_hi;
    if (lo < 0)
        return;
    d->dirty_lo = d->dirty_hi = -1;
    redraw_lines(d, lo, hi);
}

void win_redraw(Doc *d, const Rect *r)
{
    redraw_rect(d, r);
}

/* ------------------------------------------------------------------ */
/* scrolling, sliders, title                                           */
/* ------------------------------------------------------------------ */

static int clamp_scroll(Doc *d)
{
    long maxtop = tb_line_count(&d->tb) - d->page_rows;
    long maxleft = d->maxcol + 1 - d->page_cols;
    long t = d->top_line, l = d->left_col;

    if (maxtop < 0) maxtop = 0;
    if (maxleft < 0) maxleft = 0;
    if (maxleft > 1500) maxleft = 1500;
    if (t > maxtop) t = maxtop;
    if (t < 0) t = 0;
    if (l > maxleft) l = maxleft;
    if (l < 0) l = 0;
    if (t == d->top_line && l == d->left_col)
        return 0;
    d->top_line = t;
    d->left_col = l;
    return 1;
}

static int scroll_to_cursor(Doc *d)
{
    long line = tb_line_of(&d->tb, d->cursor);
    long col = doc_vcol(d, line, d->cursor);
    long t = d->top_line, l = d->left_col, m = d->page_cols / 4;

    if (line < t)
        t = line;
    else if (line >= t + d->page_rows)
        t = line - d->page_rows + 1;
    if (col < l)
        l = col - m;
    else if (col >= l + d->page_cols)
        l = col - d->page_cols + 1 + m;
    if (l < 0) l = 0;
    if (t == d->top_line && l == d->left_col)
        return 0;
    d->top_line = t;
    d->left_col = l;
    return 1;
}

static void update_sliders(Doc *d)
{
    long n = tb_line_count(&d->tb), rows = d->page_rows, cols = d->page_cols;
    long maxtop = n - rows, total = d->maxcol + 1, maxleft = total - cols;
    int vsz, vpos, hsz, hpos;

    if (maxtop < 0) maxtop = 0;
    if (maxleft < 0) maxleft = 0;
    vsz = (n <= rows) ? 1000 : (int)((rows * 1000L) / n);
    vpos = maxtop ? 1 + (int)((d->top_line * 999L) / maxtop) : 1;
    hsz = (total <= cols) ? 1000 : (int)((cols * 1000L) / total);
    hpos = maxleft ? 1 + (int)((d->left_col * 999L) / maxleft) : 1;
    if (vsz < 1) vsz = 1;
    if (hsz < 1) hsz = 1;

    if (vsz != d->c_vsz) {
        wind_set(d->whandle, WF_VSLSIZE, vsz, 0, 0, 0);
        d->c_vsz = vsz;
    }
    if (vpos != d->c_vpos) {
        wind_set(d->whandle, WF_VSLIDE, vpos, 0, 0, 0);
        d->c_vpos = vpos;
    }
    if (hsz != d->c_hsz) {
        wind_set(d->whandle, WF_HSLSIZE, hsz, 0, 0, 0);
        d->c_hsz = hsz;
    }
    if (hpos != d->c_hpos) {
        wind_set(d->whandle, WF_HSLIDE, hpos, 0, 0, 0);
        d->c_hpos = hpos;
    }
}

static void update_title(Doc *d, int force)
{
    char t[NAMELEN + 8], in[64];
    long line = tb_line_of(&d->tb, d->cursor);
    long col = doc_vcol(d, line, d->cursor);

    strcpy(t, d->name);
    if (d->modified)
        strcat(t, " *");
    if (force || strcmp(t, d->title) != 0) {
        strcpy(d->title, t);
        wset_str(d->whandle, WF_NAME, d->title);
    }
    sprintf(in, " Ln %ld  Col %ld   (%ld lines)", line + 1, col + 1,
            tb_line_count(&d->tb));
    if (force || strcmp(in, d->info) != 0) {
        strcpy(d->info, in);
        wset_str(d->whandle, WF_INFO, d->info);
    }
}

static void set_scroll(Doc *d, long t, long l)
{
    long ot = d->top_line, ol = d->left_col;
    d->top_line = t;
    d->left_col = l;
    clamp_scroll(d);
    if (ot != d->top_line || ol != d->left_col) {
        redraw_all(d);
        update_sliders(d);
    }
}

void win_layout(Doc *d)
{
    Rect w;
    if (d->whandle < 0)
        return;
    work_rect(d, &w);
    d->page_rows = w.h / app.ch;
    d->page_cols = w.w / app.cw;
    if (d->page_rows < 1) d->page_rows = 1;
    if (d->page_cols < 1) d->page_cols = 1;
    if (clamp_scroll(d))
        redraw_all(d);
    update_sliders(d);
}

void win_refresh(Doc *d)
{
    long len = tb_len(&d->tb), oc = d->last_c, oa = d->last_a;
    int full = 0;

    if (d->whandle < 0)
        return;
    if (oc > len) oc = len;
    if (oa > len) oa = len;
    doc_mark_view_change(d, oc, oa);
    if (scroll_to_cursor(d)) full = 1;
    if (clamp_scroll(d)) full = 1;
    if (full) {
        d->dirty_lo = 0;
        d->dirty_hi = LINE_ALL;
    }
    flush_dirty(d);
    d->last_c = d->cursor;
    d->last_a = d->anchor;
    update_sliders(d);
    update_title(d, 0);
}

/* Select [anchor,cursor] and make sure it is visible; if it was off screen the
 * line is centred (used by Find and by bookmark jumps). */
void win_show(Doc *d, long anchor, long cursor)
{
    long line;

    if (d->whandle < 0)
        return;
    doc_select(d, anchor, cursor);
    line = tb_line_of(&d->tb, d->cursor);
    if (line < d->top_line || line >= d->top_line + d->page_rows) {
        d->top_line = line - d->page_rows / 2;
        clamp_scroll(d);
        d->dirty_lo = 0;
        d->dirty_hi = LINE_ALL;
    }
    win_refresh(d);
}

/* ------------------------------------------------------------------ */
/* window life cycle                                                   */
/* ------------------------------------------------------------------ */

int win_open(Doc *d)
{
    gint h, x, y, w, ht, off;
    int n = app.ndocs - 1;

    h = wind_create(GADGETS, app.desk.x, app.desk.y, app.desk.w, app.desk.h);
    if (h < 0)
        return -1;
    d->whandle = h;
    d->title[0] = 0;
    d->info[0] = 0;
    d->last_c = d->last_a = 0;
    d->top_line = d->left_col = 0;
    update_title(d, 1);

    off = (gint)((n % 5) * app.cw * 2);
    x = app.desk.x + off;
    y = app.desk.y + (gint)((n % 5) * app.ch);
    w = app.desk.w - off;
    ht = app.desk.h - (gint)((n % 5) * app.ch);
    if (!wind_open(h, x, y, w, ht)) {
        wind_delete(h);
        d->whandle = -1;
        return -1;
    }
    win_layout(d);
    return 0;
}

void win_close(Doc *d)
{
    if (d->whandle >= 0) {
        wind_close(d->whandle);
        wind_delete(d->whandle);
        d->whandle = -1;
    }
}

void win_set_top(Doc *d)
{
    if (d->whandle >= 0)
        wind_set(d->whandle, WF_TOP, 0, 0, 0, 0);
}

/* ------------------------------------------------------------------ */
/* window messages                                                     */
/* ------------------------------------------------------------------ */

void win_arrowed(Doc *d, gint what)
{
    long t = d->top_line, l = d->left_col;
    long rp = d->page_rows > 1 ? d->page_rows - 1 : 1;
    long cp = d->page_cols > 1 ? d->page_cols - 1 : 1;

    switch (what) {
    case WA_UPPAGE: t -= rp; break;
    case WA_DNPAGE: t += rp; break;
    case WA_UPLINE: t -= 1;  break;
    case WA_DNLINE: t += 1;  break;
    case WA_LFPAGE: l -= cp; break;
    case WA_RTPAGE: l += cp; break;
    case WA_LFLINE: l -= 1;  break;
    case WA_RTLINE: l += 1;  break;
    }
    set_scroll(d, t, l);
}

void win_vslid(Doc *d, gint pos)
{
    long maxtop = tb_line_count(&d->tb) - d->page_rows;
    if (maxtop < 0) maxtop = 0;
    set_scroll(d, ((long)(pos - 1) * maxtop) / 999L, d->left_col);
}

void win_hslid(Doc *d, gint pos)
{
    long maxleft = d->maxcol + 1 - d->page_cols;
    if (maxleft < 0) maxleft = 0;
    set_scroll(d, d->top_line, ((long)(pos - 1) * maxleft) / 999L);
}

void win_set_rect(Doc *d, gint x, gint y, gint w, gint h)
{
    if (w < MIN_W) w = MIN_W;
    if (h < MIN_H) h = MIN_H;
    if (y < app.desk.y) y = app.desk.y;
    wind_set(d->whandle, WF_CURRXYWH, x, y, w, h);
    win_layout(d);
}

void win_fulled(Doc *d)
{
    Rect c, p, f, *target;
    wind_get(d->whandle, WF_CURRXYWH, &c.x, &c.y, &c.w, &c.h);
    wind_get(d->whandle, WF_PREVXYWH, &p.x, &p.y, &p.w, &p.h);
    wind_get(d->whandle, WF_FULLXYWH, &f.x, &f.y, &f.w, &f.h);
    if (c.x == f.x && c.y == f.y && c.w == f.w && c.h == f.h)
        target = &p;
    else
        target = &f;
    wind_set(d->whandle, WF_CURRXYWH, target->x, target->y, target->w, target->h);
    win_layout(d);
}

/* ------------------------------------------------------------------ */
/* mouse                                                               */
/* ------------------------------------------------------------------ */

static long pos_at(Doc *d, const Rect *w, gint mx, gint my)
{
    long line = d->top_line + floor_div((long)my - w->y, app.ch);
    long col = d->left_col + floor_div((long)mx - w->x + app.cw / 2, app.cw);
    if (line < 0) line = 0;
    if (line >= tb_line_count(&d->tb)) line = tb_line_count(&d->tb) - 1;
    if (col < 0) col = 0;
    return doc_pos_at_col(d, line, col);
}

int win_click(Doc *d, gint mx, gint my, gint ks, gint clicks)
{
    Rect w;
    gint x, y, b, k, lx = -1, ly = -1;
    long pos;
    int outside;

    work_rect(d, &w);
    if (mx < w.x || my < w.y || mx >= w.x + w.w || my >= w.y + w.h)
        return 0;

    pos = pos_at(d, &w, mx, my);
    if (clicks > 1) {
        doc_select_word(d, pos);
        win_refresh(d);
        return 1;
    }
    doc_set_cursor(d, pos, (ks & KS_SHIFT) != 0);
    win_refresh(d);

    for (;;) {                                  /* drag selection */
        graf_mkstate(&x, &y, &b, &k);
        if (!(b & 1))
            break;
        outside = (x < w.x || y < w.y || x >= w.x + w.w || y >= w.y + w.h);
        if (x != lx || y != ly || outside) {
            lx = x;
            ly = y;
            pos = pos_at(d, &w, x, y);
            if (pos != d->cursor || outside) {
                doc_set_cursor(d, pos, 1);
                win_refresh(d);
            }
            if (outside)
                evnt_timer(40, 0);              /* throttle auto-scroll */
        }
    }
    return 1;
}
