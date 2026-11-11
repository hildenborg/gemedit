/* bookmark.h - ten global bookmarks (F1..F10), each tied to a document.
 * A bookmark stores the offset of the START OF A LINE and is adjusted on
 * every insert/delete so it stays on the same text. */
#ifndef BOOKMARK_H
#define BOOKMARK_H

#include "doc.h"

#define BM_MAX 10

typedef struct {
    Doc  *doc;                  /* 0 = slot unused */
    long  pos;                  /* offset of first char of the bookmarked line */
} Bookmark;

extern Bookmark bm_tab[BM_MAX];

void bm_set(int slot, Doc *d, long pos);
void bm_clear_all(void);
void bm_remove_doc(Doc *d);                 /* when a document is closed */
long bm_line(int slot);                     /* current 0-based line number */

/* hooks called by doc.c after the text buffer changed */
void bm_after_insert(Doc *d, long pos, const char *s, long n);
void bm_after_delete(Doc *d, long pos, long n);

#endif
