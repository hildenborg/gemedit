/* undo.c - change stack bookkeeping (no knowledge of the text buffer) */
#include <stdlib.h>
#include <string.h>
#include "undo.h"

void undo_init(UndoStack *u)
{
    memset(u, 0, sizeof *u);
}

static void free_rec(UndoRec *r)
{
    if (r) {
        free(r->text);
        free(r);
    }
}

void undo_clear(UndoStack *u)
{
    int i;
    for (i = 0; i < u->count; i++)
        free_rec(u->rec[i]);
    u->count = u->top = 0;
    u->no_coalesce = 1;
}

static void drop_oldest(UndoStack *u)
{
    free_rec(u->rec[0]);
    memmove(&u->rec[0], &u->rec[1], (u->count - 1) * sizeof(UndoRec *));
    u->count--;
    if (u->top > 0)
        u->top--;
}

UndoRec *undo_push(UndoStack *u, int type, long pos, const char *text, long len,
                   long cur_before, long cur_after, int chain)
{
    UndoRec *r;
    int i;

    for (i = u->top; i < u->count; i++)     /* new edit kills the redo list */
        free_rec(u->rec[i]);
    u->count = u->top;

    while (u->count >= UNDO_MAX)
        drop_oldest(u);
    while (u->count > 0 && u->rec[0]->chain)    /* never keep half a group */
        drop_oldest(u);

    r = (UndoRec *)malloc(sizeof *r);
    if (!r) {
        undo_clear(u);
        return 0;
    }
    r->text = (char *)malloc(len > 0 ? len : 1);
    if (!r->text) {
        free(r);
        undo_clear(u);
        return 0;
    }
    memcpy(r->text, text, len);
    r->type = type;
    r->pos = pos;
    r->len = len;
    r->cur_before = cur_before;
    r->cur_after = cur_after;
    r->chain = chain;
    u->rec[u->count++] = r;
    u->top = u->count;
    u->no_coalesce = 0;
    return r;
}

static UndoRec *top_rec(UndoStack *u)
{
    if (u->no_coalesce || u->top == 0 || u->top != u->count)
        return 0;
    return u->rec[u->top - 1];
}

int undo_coalesce_ins(UndoStack *u, long pos, const char *s, long n, long cur_after)
{
    UndoRec *r = top_rec(u);
    char *nt;
    if (!r || r->type != UNDO_INS || r->chain || s[0] == '\n')
        return 0;
    if (r->pos + r->len != pos || r->len >= UNDO_COALESCE_MAX)
        return 0;
    nt = (char *)realloc(r->text, r->len + n);
    if (!nt)
        return 0;
    memcpy(nt + r->len, s, n);
    r->text = nt;
    r->len += n;
    r->cur_after = cur_after;
    return 1;
}

int undo_coalesce_del_back(UndoStack *u, long pos, const char *s, long n)
{
    UndoRec *r = top_rec(u);
    char *nt;
    if (!r || r->type != UNDO_DEL || r->chain || r->pos != pos + n ||
        r->len >= UNDO_COALESCE_MAX)
        return 0;
    nt = (char *)malloc(r->len + n);
    if (!nt)
        return 0;
    memcpy(nt, s, n);
    memcpy(nt + n, r->text, r->len);
    free(r->text);
    r->text = nt;
    r->len += n;
    r->pos = pos;
    r->cur_after = pos;
    return 1;
}

int undo_coalesce_del_fwd(UndoStack *u, long pos, const char *s, long n)
{
    UndoRec *r = top_rec(u);
    char *nt;
    if (!r || r->type != UNDO_DEL || r->chain || r->pos != pos ||
        r->len >= UNDO_COALESCE_MAX)
        return 0;
    nt = (char *)realloc(r->text, r->len + n);
    if (!nt)
        return 0;
    memcpy(nt + r->len, s, n);
    r->text = nt;
    r->len += n;
    return 1;
}
