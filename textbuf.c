/* textbuf.c - gap buffer with incrementally maintained line table */
#include <stdlib.h>
#include <string.h>
#include "textbuf.h"

#define GAP_GROW 4096L

int tb_init(TextBuf *t)
{
    t->cap   = GAP_GROW;
    t->buf   = (char *)malloc(t->cap);
    t->lcap  = 256;
    t->lines = (long *)malloc(t->lcap * sizeof(long));
    if (!t->buf || !t->lines) {
        free(t->buf);
        free(t->lines);
        t->buf = 0;
        t->lines = 0;
        return -1;
    }
    t->gs = 0;
    t->ge = t->cap;
    t->nlines = 1;
    t->lines[0] = 0;
    return 0;
}

void tb_free(TextBuf *t)
{
    free(t->buf);
    free(t->lines);
    t->buf = 0;
    t->lines = 0;
}

long tb_len(const TextBuf *t)
{
    return t->cap - (t->ge - t->gs);
}

int tb_at(const TextBuf *t, long pos)
{
    if (pos < t->gs)
        return (unsigned char)t->buf[pos];
    return (unsigned char)t->buf[pos + (t->ge - t->gs)];
}

static void move_gap(TextBuf *t, long pos)
{
    long n;
    if (pos < t->gs) {
        n = t->gs - pos;
        memmove(t->buf + t->ge - n, t->buf + pos, n);
        t->gs -= n;
        t->ge -= n;
    } else if (pos > t->gs) {
        n = pos - t->gs;
        memmove(t->buf + t->gs, t->buf + t->ge, n);
        t->gs += n;
        t->ge += n;
    }
}

static int ensure_gap(TextBuf *t, long n)
{
    long glen = t->ge - t->gs, extra, tail;
    char *nb;
    if (glen >= n)
        return 0;
    extra = n - glen + GAP_GROW;
    nb = (char *)realloc(t->buf, t->cap + extra);
    if (!nb)
        return -1;
    t->buf = nb;
    tail = t->cap - t->ge;
    memmove(t->buf + t->ge + extra, t->buf + t->ge, tail);
    t->ge += extra;
    t->cap += extra;
    return 0;
}

static int lines_reserve(TextBuf *t, long need)
{
    long nc;
    long *nl;
    if (need <= t->lcap)
        return 0;
    nc = t->lcap * 2;
    if (nc < need)
        nc = need + 256;
    nl = (long *)realloc(t->lines, nc * sizeof(long));
    if (!nl)
        return -1;
    t->lines = nl;
    t->lcap = nc;
    return 0;
}

long tb_line_count(const TextBuf *t) { return t->nlines; }

long tb_line_start(const TextBuf *t, long line) { return t->lines[line]; }

long tb_line_end(const TextBuf *t, long line)
{
    if (line + 1 < t->nlines)
        return t->lines[line + 1] - 1;
    return tb_len(t);
}

long tb_line_of(const TextBuf *t, long pos)
{
    long lo = 0, hi = t->nlines - 1, mid;
    while (lo < hi) {
        mid = (lo + hi + 1) / 2;
        if (t->lines[mid] <= pos)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

int tb_insert(TextBuf *t, long pos, const char *s, long n)
{
    long i, k = 0, l, j, q;

    if (n <= 0)
        return 0;
    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            k++;
    if (lines_reserve(t, t->nlines + k))
        return -1;
    if (ensure_gap(t, n))
        return -1;

    l = tb_line_of(t, pos);          /* line table still describes old text */
    move_gap(t, pos);
    memcpy(t->buf + t->gs, s, n);
    t->gs += n;

    if (k)
        memmove(&t->lines[l + 1 + k], &t->lines[l + 1],
                (t->nlines - l - 1) * sizeof(long));
    for (j = l + 1 + k; j < t->nlines + k; j++)
        t->lines[j] += n;
    q = l + 1;
    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            t->lines[q++] = pos + i + 1;
    t->nlines += k;
    return 0;
}

void tb_delete(TextBuf *t, long pos, long n)
{
    long l1, l2, rem, j;

    if (n <= 0)
        return;
    l1 = tb_line_of(t, pos);
    l2 = tb_line_of(t, pos + n);
    rem = l2 - l1;
    if (rem)
        memmove(&t->lines[l1 + 1], &t->lines[l2 + 1],
                (t->nlines - l2 - 1) * sizeof(long));
    t->nlines -= rem;
    for (j = l1 + 1; j < t->nlines; j++)
        t->lines[j] -= n;

    move_gap(t, pos);
    t->ge += n;
}

void tb_copy(const TextBuf *t, long pos, long n, char *dst)
{
    long glen = t->ge - t->gs, a;
    if (pos < t->gs) {
        a = t->gs - pos;
        if (a > n)
            a = n;
        memcpy(dst, t->buf + pos, a);
        dst += a;
        pos += a;
        n -= a;
    }
    if (n > 0)
        memcpy(dst, t->buf + pos + glen, n);
}

const char *tb_flat(TextBuf *t)
{
    move_gap(t, tb_len(t));
    return t->buf;
}
