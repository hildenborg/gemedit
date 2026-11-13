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

    /* column (box) selection, Ctrl + arrows / mouse.  The box runs from the anchor
     * corner (cs_al, cs_avc) to the caret corner (cs_cl, cs_cvc) in visual
     * columns.  Every line from min(cs_al,cs_cl) has a caret and selection
     * stored as two offsets from the line start in cs_off[2*i], cs_off[2*i+1]
     * (anchor, caret); -1 = line too short, no caret.  While active,
     * cursor == anchor == the caret corner (used for scrolling and info). */
    int   colsel;
    int   cs_busy;              /* edits are part of a column operation      */
    long  cs_al, cs_cl;
    long  cs_avc, cs_cvc;
    long *cs_off;
    long  cs_cap;               /* lines allocated in cs_off                 */

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
void  doc_mark_lines(Doc *d, long lo, long hi);    /* add lines to the redraw range */

/* selection / cursor */
int   doc_sel_range(const Doc *d, long *s, long *e);   /* returns 1 if non-empty */
int   doc_has_sel(const Doc *d);       /* normal or column selection non-empty */
void  doc_set_cursor(Doc *d, long pos, int extend);
void  doc_select(Doc *d, long a, long b);
void  doc_select_all(Doc *d);
void  doc_select_word(Doc *d, long pos);
void  doc_move(Doc *d, int kind, int extend);

/* column selection */
void  doc_col_select_to(Doc *d, long line, long vcol);  /* box from anchor to here */
void  doc_col_extend(Doc *d, long dline, long dvcol);   /* move the caret corner   */
void  doc_col_off(Doc *d);                              /* back to a single caret  */
int   doc_col_line(const Doc *d, long line, long *s, long *e, long *caret);

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
