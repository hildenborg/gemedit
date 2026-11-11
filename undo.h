/* undo.h - change stack (undo/redo) */
#ifndef UNDO_H
#define UNDO_H

#define UNDO_MAX            2048     /* records; far more than the 100 steps required */
#define UNDO_COALESCE_MAX   64      /* max chars merged into one typing step */

#define UNDO_INS 0                  /* text was inserted at pos   */
#define UNDO_DEL 1                  /* text was removed from pos  */

typedef struct {
    int   type;
    long  pos, len;
    char *text;
    long  cur_before, cur_after;
    int   chain;                    /* 1 = belongs to the previous record (one user step) */
} UndoRec;

typedef struct {
    UndoRec *rec[UNDO_MAX];
    int count;                      /* records stored                */
    int top;                        /* rec[0..top) undoable, rest redoable */
    int no_coalesce;
} UndoStack;

void     undo_init(UndoStack *u);
void     undo_clear(UndoStack *u);
UndoRec *undo_push(UndoStack *u, int type, long pos, const char *text, long len,
                   long cur_before, long cur_after, int chain);
int      undo_coalesce_ins(UndoStack *u, long pos, const char *s, long n, long cur_after);
int      undo_coalesce_del_back(UndoStack *u, long pos, const char *s, long n);
int      undo_coalesce_del_fwd(UndoStack *u, long pos, const char *s, long n);

#define undo_can_undo(u) ((u)->top > 0)
#define undo_can_redo(u) ((u)->top < (u)->count)

#endif
