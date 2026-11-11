/* doc.h - a document: text, cursor/selection, undo history, file name.
 * No GEM calls in here; the window layer reads the view fields. */
#ifndef DOC_H
#define DOC_H

#include "textbuf.h"
#include "undo.h"

#define PATHLEN   128
#define NAMELEN   48
#define TABW      4
#define LINE_ALL  0x7FFFFFFFL

typedef struct Doc {
    TextBuf   tb;
    UndoStack undo;

    char path[PATHLEN];
    char name[NAMELEN];
    int  has_path;
    int  modified;
    int  error;                 /* set on out-of-memory; UI reports + clears */

    long cursor, anchor;        /* selection = [min,max) between them        */
    long want_col;              /* remembered column for up/down, -1 = none  */
    long maxcol;                /* widest line seen (for horizontal slider)  */

    long dirty_lo, dirty_hi;    /* lines needing redraw, -1 = none           */

    /* view state, owned by window.c */
    int  whandle;
    long top_line, left_col;
    long page_rows, page_cols;
    long last_c, last_a;        /* cursor/anchor as last drawn               */
    int  c_vpos, c_vsz, c_hpos, c_hsz;
    char title[NAMELEN + 8];
    char info[64];
} Doc;

enum { MV_LEFT, MV_RIGHT, MV_UP, MV_DOWN, MV_HOME, MV_END,
       MV_WORDL, MV_WORDR, MV_TOP, MV_BOTTOM, MV_PGUP, MV_PGDN };

Doc  *doc_new(void);
void  doc_free(Doc *d);
void  doc_set_path(Doc *d, const char *path);
void  doc_set_untitled(Doc *d, int n);

int   doc_load(Doc *d, const char *path);   /* 0 ok, -1 open/read error, -2 no memory */
int   doc_save(Doc *d, const char *path);   /* 0 ok, -1 error; updates name/path     */

/* geometry helpers (tab aware) */
long  doc_vcol(Doc *d, long line, long pos);
long  doc_pos_at_col(Doc *d, long line, long col);
void  doc_mark_view_change(Doc *d, long old_cursor, long old_anchor);

/* selection / cursor */
int   doc_sel_range(const Doc *d, long *s, long *e);   /* returns 1 if non-empty */
void  doc_set_cursor(Doc *d, long pos, int extend);
void  doc_select(Doc *d, long a, long b);
void  doc_select_all(Doc *d);
void  doc_select_word(Doc *d, long pos);
void  doc_move(Doc *d, int kind, int extend);

/* editing */
void  doc_type(Doc *d, int ch);
void  doc_newline(Doc *d);
void  doc_backspace(Doc *d);
void  doc_delete(Doc *d);
int   doc_copy(Doc *d);
int   doc_cut(Doc *d);
int   doc_paste(Doc *d);
void  doc_indent(Doc *d);       /* add one tab to the selected lines    */
void  doc_unindent(Doc *d);     /* remove one indent from selected/current line */
int   doc_undo(Doc *d);
int   doc_redo(Doc *d);

/* search */
long  doc_find(Doc *d, const char *needle, int match_case, long from);        /* wraps */
long  doc_find_nowrap(Doc *d, const char *needle, int match_case, long from);
int   doc_replace_sel(Doc *d, const char *needle, const char *repl, int match_case);
long  doc_replace_all(Doc *d, const char *needle, const char *repl, int match_case);

#endif
