/* doc.c - document model and all editing logic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "doc.h"
#include "clip.h"
#include "bookmark.h"

static int isw(int c) { return isalnum(c) || c == '_' || c >= 128; }

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void mark(Doc *d, long lo, long hi)
{
    if (d->dirty_lo < 0 || lo < d->dirty_lo)
        d->dirty_lo = lo;
    if (hi > d->dirty_hi)
        d->dirty_hi = hi;
}

static void note_width(Doc *d, long line)
{
    long s = tb_line_start(&d->tb, line), e = tb_line_end(&d->tb, line);
    long p, col = 0;
    for (p = s; p < e; p++)
        col += (tb_at(&d->tb, p) == '\t') ? TABW - (col % TABW) : 1;
    if (col > d->maxcol)
        d->maxcol = col;
}

static void recalc_maxcol(Doc *d)
{
    long i, n = tb_line_count(&d->tb);
    d->maxcol = 0;
    for (i = 0; i < n; i++)
        note_width(d, i);
}

long doc_vcol(Doc *d, long line, long pos)
{
    long p, col = 0;
    for (p = tb_line_start(&d->tb, line); p < pos; p++)
        col += (tb_at(&d->tb, p) == '\t') ? TABW - (col % TABW) : 1;
    return col;
}

long doc_pos_at_col(Doc *d, long line, long col)
{
    long p = tb_line_start(&d->tb, line), e = tb_line_end(&d->tb, line);
    long c = 0, nc;
    while (p < e) {
        nc = c + ((tb_at(&d->tb, p) == '\t') ? TABW - (c % TABW) : 1);
        if (nc > col)
            break;
        c = nc;
        p++;
    }
    return p;
}

int doc_sel_range(const Doc *d, long *s, long *e)
{
    if (d->anchor < d->cursor) {
        if (s) *s = d->anchor;
        if (e) *e = d->cursor;
    } else {
        if (s) *s = d->cursor;
        if (e) *e = d->anchor;
    }
    return d->anchor != d->cursor;
}

/* Called by the window layer: mark every line whose cursor/selection
 * look changed since the last draw. */
void doc_mark_view_change(Doc *d, long oc, long oa)
{
    long nc = d->cursor, na = d->anchor, lo, hi;
    if (oc == nc && oa == na)
        return;
    if (oa == na) {
        lo = oc < nc ? oc : nc;
        hi = oc < nc ? nc : oc;
    } else {
        lo = hi = oc;
        if (oa < lo) lo = oa;
        if (nc < lo) lo = nc;
        if (na < lo) lo = na;
        if (oa > hi) hi = oa;
        if (nc > hi) hi = nc;
        if (na > hi) hi = na;
    }
    mark(d, tb_line_of(&d->tb, lo), tb_line_of(&d->tb, hi));
}

/* ------------------------------------------------------------------ */
/* life cycle                                                          */
/* ------------------------------------------------------------------ */

Doc *doc_new(void)
{
    Doc *d = (Doc *)calloc(1, sizeof(Doc));
    if (!d)
        return 0;
    if (tb_init(&d->tb)) {
        free(d);
        return 0;
    }
    undo_init(&d->undo);
    d->want_col = -1;
    d->dirty_lo = d->dirty_hi = -1;
    d->whandle = -1;
    d->c_vpos = d->c_vsz = d->c_hpos = d->c_hsz = -1;
    d->page_rows = 20;
    d->page_cols = 60;
    return d;
}

void doc_free(Doc *d)
{
    if (!d)
        return;
    undo_clear(&d->undo);
    tb_free(&d->tb);
    free(d);
}

void doc_set_path(Doc *d, const char *path)
{
    const char *p = path, *q;
    strncpy(d->path, path, PATHLEN - 1);
    d->path[PATHLEN - 1] = 0;
    for (q = path; *q; q++)
        if (*q == '\\' || *q == '/' || *q == ':')
            p = q + 1;
    strncpy(d->name, p, NAMELEN - 1);
    d->name[NAMELEN - 1] = 0;
    d->has_path = 1;
}

void doc_set_untitled(Doc *d, int n)
{
    sprintf(d->name, "Untitled %d", n);
    d->path[0] = 0;
    d->has_path = 0;
}

/* ------------------------------------------------------------------ */
/* file I/O  (stored internally with \n, files use \r\n)              */
/* ------------------------------------------------------------------ */

int doc_load(Doc *d, const char *path)
{
    FILE *f = fopen(path, "rb");
    long sz, i, o = 0;
    char *tmp, c;

    if (!f)
        return -1;
    fseek(f, 0L, SEEK_END);
    sz = ftell(f);
    fseek(f, 0L, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    tmp = (char *)malloc(sz + 1);
    if (!tmp) {
        fclose(f);
        return -2;
    }
    if (sz > 0 && fread(tmp, 1, sz, f) != (size_t)sz) {
        free(tmp);
        fclose(f);
        return -1;
    }
    fclose(f);

    for (i = 0; i < sz; i++) {
        c = tmp[i];
        if (c == '\r') {
            if (i + 1 < sz && tmp[i + 1] == '\n')
                continue;               /* CRLF -> LF */
            c = '\n';                   /* lone CR -> LF */
        }
        tmp[o++] = c;
    }
    if (tb_insert(&d->tb, 0L, tmp, o)) {
        free(tmp);
        return -2;
    }
    free(tmp);
    d->cursor = d->anchor = 0;
    d->modified = 0;
    undo_clear(&d->undo);
    recalc_maxcol(d);
    return 0;
}

int doc_save(Doc *d, const char *path)
{
    FILE *f = fopen(path, "wb");
    const char *t;
    long len, i, o = 0;
    char out[1030];

    if (!f)
        return -1;
    t = tb_flat(&d->tb);
    len = tb_len(&d->tb);
    for (i = 0; i < len; i++) {
        if (t[i] == '\n') {
            out[o++] = '\r';
            out[o++] = '\n';
        } else
            out[o++] = t[i];
        if (o >= 1024) {
            if (fwrite(out, 1, o, f) != (size_t)o) {
                fclose(f);
                return -1;
            }
            o = 0;
        }
    }
    if (o && fwrite(out, 1, o, f) != (size_t)o) {
        fclose(f);
        return -1;
    }
    if (fclose(f))
        return -1;
    doc_set_path(d, path);
    d->modified = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* primitive edits (buffer + dirty marks), and undo-recording edits    */
/* ------------------------------------------------------------------ */

static int raw_insert(Doc *d, long pos, const char *s, long n)
{
    long l = tb_line_of(&d->tb, pos), k = 0, i;
    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            k++;
    if (tb_insert(&d->tb, pos, s, n)) {
        d->error = 1;
        return -1;
    }
    bm_after_insert(d, pos, s, n);
    mark(d, l, k ? LINE_ALL : l);
    d->modified = 1;
    for (i = l; i <= l + k && i < tb_line_count(&d->tb); i++)
        note_width(d, i);
    return 0;
}

static void raw_delete(Doc *d, long pos, long n)
{
    long l1 = tb_line_of(&d->tb, pos), l2 = tb_line_of(&d->tb, pos + n);
    tb_delete(&d->tb, pos, n);
    bm_after_delete(d, pos, n);
    mark(d, l1, l2 > l1 ? LINE_ALL : l1);
    d->modified = 1;
}

static void edit_insert(Doc *d, long pos, const char *s, long n, int chain, int typed)
{
    long cb = d->cursor;
    if (n <= 0)
        return;
    if (raw_insert(d, pos, s, n))
        return;
    d->cursor = d->anchor = pos + n;
    if (typed && !chain && undo_coalesce_ins(&d->undo, pos, s, n, pos + n))
        return;
    undo_push(&d->undo, UNDO_INS, pos, s, n, cb, pos + n, chain);
}

/* mode: 0 = no coalescing, 1 = backspace, 2 = forward delete */
static void edit_delete(Doc *d, long pos, long n, int chain, int mode)
{
    long cb = d->cursor;
    char *tmp;
    if (n <= 0)
        return;
    tmp = (char *)malloc(n);
    if (tmp)
        tb_copy(&d->tb, pos, n, tmp);
    raw_delete(d, pos, n);
    d->cursor = d->anchor = pos;
    if (!tmp) {
        undo_clear(&d->undo);
        d->error = 1;
        return;
    }
    if (!chain && mode == 1 && undo_coalesce_del_back(&d->undo, pos, tmp, n)) {
        free(tmp);
        return;
    }
    if (!chain && mode == 2 && undo_coalesce_del_fwd(&d->undo, pos, tmp, n)) {
        free(tmp);
        return;
    }
    undo_push(&d->undo, UNDO_DEL, pos, tmp, n, cb, pos, chain);
    free(tmp);
}

/* ------------------------------------------------------------------ */
/* public editing commands                                             */
/* ------------------------------------------------------------------ */

void doc_type(Doc *d, int ch)
{
    char c = (char)ch;
    long s, e;
    if (doc_sel_range(d, &s, &e)) {
        edit_delete(d, s, e - s, 0, 0);
        edit_insert(d, s, &c, 1, 1, 0);
    } else
        edit_insert(d, d->cursor, &c, 1, 0, 1);
    d->want_col = -1;
}

void doc_newline(Doc *d)
{
    char buf[136];
    long s, e, ls, p;
    int n = 1, had = 0, c;

    if (doc_sel_range(d, &s, &e)) {
        edit_delete(d, s, e - s, 0, 0);
        had = 1;
    }
    buf[0] = '\n';
    ls = tb_line_start(&d->tb, tb_line_of(&d->tb, d->cursor));
    for (p = ls; p < d->cursor && n < 128; p++) {       /* auto indent */
        c = tb_at(&d->tb, p);
        if (c != ' ' && c != '\t')
            break;
        buf[n++] = (char)c;
    }
    edit_insert(d, d->cursor, buf, n, had, 0);
    d->want_col = -1;
}

void doc_backspace(Doc *d)
{
    long s, e;
    if (doc_sel_range(d, &s, &e))
        edit_delete(d, s, e - s, 0, 0);
    else if (d->cursor > 0)
        edit_delete(d, d->cursor - 1, 1, 0, 1);
    d->want_col = -1;
}

void doc_delete(Doc *d)
{
    long s, e;
    if (doc_sel_range(d, &s, &e))
        edit_delete(d, s, e - s, 0, 0);
    else if (d->cursor < tb_len(&d->tb))
        edit_delete(d, d->cursor, 1, 0, 2);
    d->want_col = -1;
}

int doc_copy(Doc *d)
{
    long s, e;
    char *tmp;
    int r;
    if (!doc_sel_range(d, &s, &e))
        return 0;
    tmp = (char *)malloc(e - s);
    if (!tmp) {
        d->error = 1;
        return 0;
    }
    tb_copy(&d->tb, s, e - s, tmp);
    r = clip_set(tmp, e - s);
    free(tmp);
    if (r)
        d->error = 1;
    return r == 0;
}

int doc_cut(Doc *d)
{
    long s, e;
    if (!doc_copy(d))
        return 0;
    doc_sel_range(d, &s, &e);
    edit_delete(d, s, e - s, 0, 0);
    d->want_col = -1;
    return 1;
}

int doc_paste(Doc *d)
{
    long n = clip_len(), s, e;
    int had = 0;
    if (n <= 0)
        return 0;
    if (doc_sel_range(d, &s, &e)) {
        edit_delete(d, s, e - s, 0, 0);
        had = 1;
    }
    edit_insert(d, d->cursor, clip_get(), n, had, 0);
    d->want_col = -1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* undo / redo                                                         */
/* ------------------------------------------------------------------ */

static void revert(Doc *d, UndoRec *r)
{
    if (r->type == UNDO_INS)
        raw_delete(d, r->pos, r->len);
    else
        raw_insert(d, r->pos, r->text, r->len);
    d->cursor = d->anchor = r->cur_before;
}

static void apply(Doc *d, UndoRec *r)
{
    if (r->type == UNDO_INS)
        raw_insert(d, r->pos, r->text, r->len);
    else
        raw_delete(d, r->pos, r->len);
    d->cursor = d->anchor = r->cur_after;
}

int doc_undo(Doc *d)
{
    UndoStack *u = &d->undo;
    UndoRec *r;
    if (u->top <= 0)
        return 0;
    do {
        r = u->rec[--u->top];
        revert(d, r);
    } while (r->chain && u->top > 0);
    u->no_coalesce = 1;
    d->want_col = -1;
    return 1;
}

int doc_redo(Doc *d)
{
    UndoStack *u = &d->undo;
    if (u->top >= u->count)
        return 0;
    apply(d, u->rec[u->top++]);
    while (u->top < u->count && u->rec[u->top]->chain)
        apply(d, u->rec[u->top++]);
    u->no_coalesce = 1;
    d->want_col = -1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* cursor movement and selection                                       */
/* ------------------------------------------------------------------ */

static void set_cursor(Doc *d, long pos, int extend, int keep_want)
{
    long len = tb_len(&d->tb);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    d->cursor = pos;
    if (!extend)
        d->anchor = pos;
    if (!keep_want)
        d->want_col = -1;
    d->undo.no_coalesce = 1;
}

void doc_set_cursor(Doc *d, long pos, int extend)
{
    set_cursor(d, pos, extend, 0);
}

void doc_select(Doc *d, long a, long b)
{
    long len = tb_len(&d->tb);
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (a > len) a = len;
    if (b > len) b = len;
    d->anchor = a;
    d->cursor = b;
    d->want_col = -1;
    d->undo.no_coalesce = 1;
}

void doc_select_all(Doc *d)
{
    doc_select(d, 0L, tb_len(&d->tb));
}

void doc_select_word(Doc *d, long pos)
{
    long len = tb_len(&d->tb), a, b;
    if (!(pos < len && isw(tb_at(&d->tb, pos)))) {
        if (pos > 0 && isw(tb_at(&d->tb, pos - 1)))
            pos--;
        else
            return;
    }
    a = b = pos;
    while (a > 0 && isw(tb_at(&d->tb, a - 1)))
        a--;
    while (b < len && isw(tb_at(&d->tb, b)))
        b++;
    doc_select(d, a, b);
}

void doc_move(Doc *d, int kind, int extend)
{
    long pos = d->cursor, len = tb_len(&d->tb), line, s, e, n;
    int hassel = doc_sel_range(d, &s, &e), vertical = 0;

    switch (kind) {
    case MV_LEFT:
        if (hassel && !extend) pos = s;
        else if (pos > 0) pos--;
        break;
    case MV_RIGHT:
        if (hassel && !extend) pos = e;
        else if (pos < len) pos++;
        break;
    case MV_UP: case MV_DOWN: case MV_PGUP: case MV_PGDN:
        n = (kind == MV_UP || kind == MV_DOWN) ? 1 :
            (d->page_rows > 1 ? d->page_rows - 1 : 1);
        line = tb_line_of(&d->tb, pos);
        if (d->want_col < 0)
            d->want_col = doc_vcol(d, line, pos);
        line += (kind == MV_UP || kind == MV_PGUP) ? -n : n;
        if (line < 0)
            pos = 0;
        else if (line >= tb_line_count(&d->tb))
            pos = len;
        else
            pos = doc_pos_at_col(d, line, d->want_col);
        vertical = 1;
        break;
    case MV_HOME:
        pos = tb_line_start(&d->tb, tb_line_of(&d->tb, pos));
        break;
    case MV_END:
        pos = tb_line_end(&d->tb, tb_line_of(&d->tb, pos));
        break;
    case MV_WORDL:
        while (pos > 0 && !isw(tb_at(&d->tb, pos - 1))) pos--;
        while (pos > 0 && isw(tb_at(&d->tb, pos - 1))) pos--;
        break;
    case MV_WORDR:
        while (pos < len && !isw(tb_at(&d->tb, pos))) pos++;
        while (pos < len && isw(tb_at(&d->tb, pos))) pos++;
        break;
    case MV_TOP:    pos = 0;   break;
    case MV_BOTTOM: pos = len; break;
    }
    set_cursor(d, pos, extend, vertical);
}

/* ------------------------------------------------------------------ */
/* search / replace                                                    */
/* ------------------------------------------------------------------ */

static int match_at(const char *t, const char *nd, long nl, int mc)
{
    long i;
    if (mc)
        return memcmp(t, nd, nl) == 0;
    for (i = 0; i < nl; i++)
        if (tolower((unsigned char)t[i]) != tolower((unsigned char)nd[i]))
            return 0;
    return 1;
}

static long find_in(const char *t, long len, long from, const char *nd, long nl, int mc)
{
    long p;
    for (p = from; p + nl <= len; p++)
        if (match_at(t + p, nd, nl, mc))
            return p;
    return -1;
}

long doc_find_nowrap(Doc *d, const char *needle, int mc, long from)
{
    long nl = (long)strlen(needle), len = tb_len(&d->tb);
    const char *t;
    if (nl == 0 || nl > len)
        return -1;
    t = tb_flat(&d->tb);
    if (from < 0 || from > len)
        from = 0;
    return find_in(t, len, from, needle, nl, mc);
}

long doc_find(Doc *d, const char *needle, int mc, long from)
{
    long p = doc_find_nowrap(d, needle, mc, from);
    if (p < 0 && from > 0)
        p = doc_find_nowrap(d, needle, mc, 0L);         /* wrap around */
    return p;
}

int doc_replace_sel(Doc *d, const char *needle, const char *repl, int mc)
{
    long s, e, nl = (long)strlen(needle);
    const char *t;
    if (!nl || !doc_sel_range(d, &s, &e) || e - s != nl)
        return 0;
    t = tb_flat(&d->tb);
    if (!match_at(t + s, needle, nl, mc))
        return 0;
    edit_delete(d, s, nl, 0, 0);
    edit_insert(d, s, repl, (long)strlen(repl), 1, 0);
    return 1;
}

long doc_replace_all(Doc *d, const char *needle, const char *repl, int mc)
{
    long nl = (long)strlen(needle), rl = (long)strlen(repl);
    long len = tb_len(&d->tb), first, m, pos, count = 0, olen = 0, ocap = 0, np;
    long q[BM_MAX], nq[BM_MAX];
    int cand[BM_MAX], mapped[BM_MAX], j;
    const char *t;
    char *out = 0, *nb;

    if (!nl)
        return 0;
    t = tb_flat(&d->tb);
    first = find_in(t, len, 0L, needle, nl, mc);
    if (first < 0)
        return 0;

    /* bookmarks inside the replaced region are remapped by hand below */
    for (j = 0; j < BM_MAX; j++) {
        q[j] = bm_tab[j].pos;
        cand[j] = (bm_tab[j].doc == d && q[j] >= first);
        mapped[j] = 0;
    }

    pos = first;
    while ((m = find_in(t, len, pos, needle, nl, mc)) >= 0) {
        long need = olen + (m - pos) + rl;
        if (!out || need > ocap) {
            ocap = need * 2 + 64;
            nb = (char *)realloc(out, ocap);
            if (!nb) {
                free(out);
                d->error = 1;
                return 0;
            }
            out = nb;
        }
        for (j = 0; j < BM_MAX; j++)            /* in the text before the match */
            if (cand[j] && !mapped[j] && q[j] >= pos && q[j] < m) {
                nq[j] = olen + (q[j] - pos);
                mapped[j] = 1;
            }
        memcpy(out + olen, t + pos, m - pos);
        olen += m - pos;
        for (j = 0; j < BM_MAX; j++)            /* inside the match itself */
            if (cand[j] && !mapped[j] && q[j] >= m && q[j] < m + nl) {
                nq[j] = (q[j] == m) ? olen : olen + rl;
                mapped[j] = 1;
            }
        memcpy(out + olen, repl, rl);
        olen += rl;
        pos = m + nl;
        count++;
    }
    for (j = 0; j < BM_MAX; j++)
        if (cand[j] && !mapped[j] && q[j] == pos) {
            nq[j] = olen;
            mapped[j] = 1;
        }

    /* whole region [first,pos) becomes one undo step */
    edit_delete(d, first, pos - first, 0, 0);
    if (olen > 0)
        edit_insert(d, first, out, olen, 1, 0);
    free(out);

    for (j = 0; j < BM_MAX; j++)
        if (mapped[j]) {
            np = first + nq[j];
            if (np > tb_len(&d->tb))
                np = tb_len(&d->tb);
            bm_tab[j].pos = tb_line_start(&d->tb, tb_line_of(&d->tb, np));
        }
    return count;
}

/* ------------------------------------------------------------------ */
/* block indent / unindent                                             */
/* ------------------------------------------------------------------ */

/* first/last line touched by the selection (a selection ending at the very
 * start of a line does not include that line). */
static int sel_lines(Doc *d, long *l1, long *l2)
{
    long s, e;
    int has = doc_sel_range(d, &s, &e);
    *l1 = tb_line_of(&d->tb, s);
    *l2 = tb_line_of(&d->tb, e);
    if (has && *l2 > *l1 && e == tb_line_start(&d->tb, *l2))
        (*l2)--;
    return has;
}

void doc_indent(Doc *d)
{
    long l1, l2, ln;
    int first = 1;

    sel_lines(d, &l1, &l2);
    for (ln = l1; ln <= l2; ln++) {
        if (tb_line_end(&d->tb, ln) == tb_line_start(&d->tb, ln))
            continue;                           /* leave blank lines alone */
        edit_insert(d, tb_line_start(&d->tb, ln), "\t", 1, !first, 0);
        first = 0;
    }
    if (first)
        return;
    d->anchor = tb_line_start(&d->tb, l1);
    d->cursor = tb_line_end(&d->tb, l2);
    d->want_col = -1;
    d->undo.no_coalesce = 1;
}

void doc_unindent(Doc *d)
{
    long l1, l2, ln, ls, le, k, oc = d->cursor;
    int has, first = 1;

    has = sel_lines(d, &l1, &l2);
    for (ln = l1; ln <= l2; ln++) {
        ls = tb_line_start(&d->tb, ln);
        le = tb_line_end(&d->tb, ln);
        k = 0;
        if (ls < le && tb_at(&d->tb, ls) == '\t')
            k = 1;
        else
            while (k < TABW && ls + k < le && tb_at(&d->tb, ls + k) == ' ')
                k++;
        if (!k)
            continue;
        edit_delete(d, ls, k, !first, 0);
        first = 0;
        if (!has && oc > ls)                    /* keep caret on its text */
            oc -= (oc - ls < k) ? oc - ls : k;
    }
    if (first)
        return;
    if (has) {
        d->anchor = tb_line_start(&d->tb, l1);
        d->cursor = tb_line_end(&d->tb, l2);
    } else
        d->cursor = d->anchor = oc;
    d->want_col = -1;
    d->undo.no_coalesce = 1;
}
