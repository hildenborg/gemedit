/* bookmark.c - bookmark table and its adjustment rules */
#include <string.h>
#include "bookmark.h"

Bookmark bm_tab[BM_MAX];

void bm_set(int slot, Doc *d, long pos)
{
    if (slot < 0 || slot >= BM_MAX)
        return;
    bm_tab[slot].doc = d;
    bm_tab[slot].pos = pos;
}

void bm_clear_all(void)
{
    memset(bm_tab, 0, sizeof bm_tab);
}

void bm_remove_doc(Doc *d)
{
    int i;
    for (i = 0; i < BM_MAX; i++)
        if (bm_tab[i].doc == d)
            bm_tab[i].doc = 0;
}

long bm_line(int slot)
{
    Bookmark *b = &bm_tab[slot];
    long len = tb_len(&b->doc->tb);
    if (b->pos > len)
        b->pos = len;
    return tb_line_of(&b->doc->tb, b->pos);
}

/* Text was inserted at 'pos'.  A bookmark after it moves along.  A bookmark
 * exactly at 'pos' stays, unless the new text contains a newline: then the
 * old line (our text) now starts after the last inserted newline. */
void bm_after_insert(Doc *d, long pos, const char *s, long n)
{
    int i;
    long k, last = -1;

    for (k = 0; k < n; k++)
        if (s[k] == '\n')
            last = k;
    for (i = 0; i < BM_MAX; i++) {
        if (bm_tab[i].doc != d)
            continue;
        if (bm_tab[i].pos > pos)
            bm_tab[i].pos += n;
        else if (bm_tab[i].pos == pos && last >= 0)
            bm_tab[i].pos = pos + last + 1;
    }
}

/* [pos,pos+n) was deleted.  Bookmarks behind it move back; one whose line
 * start was deleted collapses to the line that now contains 'pos'. */
void bm_after_delete(Doc *d, long pos, long n)
{
    int i;
    long q;

    for (i = 0; i < BM_MAX; i++) {
        if (bm_tab[i].doc != d)
            continue;
        q = bm_tab[i].pos;
        if (q > pos) {
            if (q >= pos + n)
                q -= n;
            else
                q = pos;
        }
        if (q > tb_len(&d->tb))
            q = tb_len(&d->tb);
        bm_tab[i].pos = tb_line_start(&d->tb, tb_line_of(&d->tb, q));
    }
}
