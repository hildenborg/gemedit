/* commands.c - implementation of all menu / shortcut actions */
#include "gemcompat.h"
#include "app.h"
#include "commands.h"
#include "dialog.h"
#include "menu.h"
#include "window.h"
#include "clip.h"
#include "bookmark.h"
#include "session.h"

static char find_buf[DLG_FLD + 2] = "";
static char repl_buf[DLG_FLD + 2] = "";
static int  find_case = 0;

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

void cmd_check_error(Doc *d)
{
    if (d && d->error) {
        d->error = 0;
        dlg_alert(3, "Out of memory!", "Operation incomplete.");
    }
}

static int same_path(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

static int is_pristine(Doc *d)
{
    return !d->has_path && !d->modified && tb_len(&d->tb) == 0;
}

static void remove_doc(Doc *d)
{
    win_close(d);
    app_doc_remove(d);
}

/* ------------------------------------------------------------------ */
/* file commands                                                       */
/* ------------------------------------------------------------------ */

static void cmd_new(void)
{
    Doc *d = app_doc_new();
    if (!d) {
        dlg_alert(3, app.ndocs >= MAX_DOCS ? "Too many documents open." : "Out of memory!", "");
        return;
    }
    doc_set_untitled(d, ++app.untitled_seq);
    if (win_open(d) < 0) {
        dlg_alert(3, "No more windows available.", "");
        app_doc_remove(d);
    }
}

/* Load a file into a new document with a window.  With 'quiet' errors
 * are not reported (the caller counts them).  Returns 0 on failure. */
static Doc *open_file(const char *path, int quiet)
{
    Doc *d;
    int r;

    d = app_doc_new();
    if (!d) {
        if (!quiet)
            dlg_alert(3, app.ndocs >= MAX_DOCS ? "Too many documents open." : "Out of memory!", "");
        return 0;
    }
    r = doc_load(d, path);
    if (r) {
        if (!quiet)
            dlg_alert(3, r == -2 ? "Not enough memory for file:" : "Cannot read file:", path);
        app_doc_remove(d);
        return 0;
    }
    doc_set_path(d, path);
    if (win_open(d) < 0) {
        if (!quiet)
            dlg_alert(3, "No more windows available.", "");
        app_doc_remove(d);
        return 0;
    }
    return d;
}

static void cmd_open(void)
{
    char path[PATHLEN];
    Doc *old = 0;
    int i;

    if (!dlg_fileselect("Open file", path, ""))
        return;
    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i]->has_path && same_path(app.docs[i]->path, path)) {
            win_set_top(app.docs[i]);
            return;
        }
    if (app.ndocs == 1 && is_pristine(app.docs[0]))
        old = app.docs[0];              /* replace the empty start-up document */
    if (old && app.ndocs >= MAX_DOCS)
        old = 0;

    if (open_file(path, 0) && old)
        remove_doc(old);
}

static int cmd_saveas(Doc *d)
{
    char path[PATHLEN];

    if (!dlg_fileselect("Save as", path, d->has_path ? d->name : ""))
        return 0;
    if (file_exists(path) && !(d->has_path && same_path(path, d->path)))
        if (!dlg_confirm_overwrite(path))
            return 0;
    if (doc_save(d, path)) {
        dlg_alert(3, "Could not write file:", path);
        return 0;
    }
    win_refresh(d);
    return 1;
}

static int cmd_save(Doc *d)
{
    if (!d->has_path)
        return cmd_saveas(d);
    if (doc_save(d, d->path)) {
        dlg_alert(3, "Could not write file:", d->name);
        return 0;
    }
    win_refresh(d);
    return 1;
}

/* Save every modified document; untitled ones ask for a file name. */
static void cmd_save_all(void)
{
    int i;
    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i]->modified) {
            if (!app.docs[i]->has_path)
                win_set_top(app.docs[i]);       /* show which one is being named */
            cmd_save(app.docs[i]);
        }
}

/* 1 = ok to discard/close, 0 = user cancelled (or save failed) */
static int confirm_discard(Doc *d)
{
    int r;
    if (!d->modified)
        return 1;
    win_set_top(d);
    r = dlg_ask_save(d->name);
    if (r == 1)
        return cmd_save(d);
    return r == 2;
}

int cmd_close_doc(Doc *d)
{
    if (!confirm_discard(d))
        return 0;
    remove_doc(d);
    return 1;
}

static void cmd_quit(void)
{
    int i;
    for (i = 0; i < app.ndocs; i++)
        if (!confirm_discard(app.docs[i]))
            return;
    while (app.ndocs)
        remove_doc(app.docs[0]);
    app.running = 0;
}

/* ------------------------------------------------------------------ */
/* sessions: the open documents and the bookmarks in one file          */
/* ------------------------------------------------------------------ */

static Session ses;                     /* ~2 KB, kept off the stack */

static void cmd_save_session(void)
{
    char path[PATHLEN], msg[40];
    int i, j, idx[MAX_DOCS], skipped = 0;
    Doc *d, *top = app_top_doc();
    long line;

    ses_clear(&ses);
    for (i = 0; i < app.ndocs; i++) {
        d = app.docs[i];
        idx[i] = -1;
        if (!d->has_path) {             /* untitled: nothing to reopen */
            skipped++;
            continue;
        }
        idx[i] = ses.ndocs;
        line = tb_line_of(&d->tb, d->cursor);
        strcpy(ses.doc[ses.ndocs].path, d->path);
        ses.doc[ses.ndocs].line = line;
        ses.doc[ses.ndocs].col = d->cursor - tb_line_start(&d->tb, line);
        if (d == top)
            ses.top = ses.ndocs;
        ses.ndocs++;
    }
    for (j = 0; j < BM_MAX; j++)
        for (i = 0; i < app.ndocs; i++)
            if (bm_tab[j].doc == app.docs[i] && idx[i] >= 0) {
                ses.bm_doc[j] = idx[i];
                ses.bm_line[j] = bm_line(j);
            }

    if (!dlg_fileselect("Save session", path, "EDITOR.SES"))
        return;
    if (file_exists(path) && !dlg_confirm_overwrite(path))
        return;
    if (ses_write(path, &ses)) {
        dlg_alert(3, "Could not write session:", path);
        return;
    }
    if (skipped) {
        sprintf(msg, "%d untitled document(s)", skipped);
        dlg_alert(1, msg, "were not saved in the session.");
    }
}

/* Replace the open documents and the bookmarks with those in a session file.
 * Modified documents are offered for saving first; Cancel aborts. */
static void cmd_load_session(void)
{
    char path[PATHLEN], msg[40];
    Doc *map[SES_MAXDOCS], *d;
    int i, r, failed = 0;
    long line, ls, le;

    if (!dlg_fileselect("Load session", path, ""))
        return;
    r = ses_read(path, &ses);
    if (r) {
        dlg_alert(3, r == -2 ? "Not a session file:" : "Cannot read file:", path);
        return;
    }
    for (i = 0; i < app.ndocs; i++)
        if (!confirm_discard(app.docs[i]))
            return;
    while (app.ndocs)
        remove_doc(app.docs[0]);        /* also drops their bookmarks */
    bm_clear_all();

    for (i = 0; i < ses.ndocs; i++) {
        map[i] = (app.ndocs < MAX_DOCS) ? open_file(ses.doc[i].path, 1) : 0;
        if (!map[i]) {
            failed++;
            continue;
        }
        d = map[i];
        line = ses.doc[i].line;
        if (line >= tb_line_count(&d->tb))
            line = tb_line_count(&d->tb) - 1;
        ls = tb_line_start(&d->tb, line);
        le = tb_line_end(&d->tb, line);
        ls = (ses.doc[i].col < le - ls) ? ls + ses.doc[i].col : le;
        win_show(d, ls, ls);
    }
    for (i = 0; i < BM_MAX; i++) {
        if (ses.bm_doc[i] < 0 || !(d = map[ses.bm_doc[i]]))
            continue;
        line = ses.bm_line[i];
        if (line >= tb_line_count(&d->tb))
            line = tb_line_count(&d->tb) - 1;
        bm_set(i, d, tb_line_start(&d->tb, line));
    }
    if (ses.top >= 0 && map[ses.top])
        win_set_top(map[ses.top]);
    if (failed) {
        sprintf(msg, "%d file(s) could not be opened.", failed);
        dlg_alert(1, "Session loaded, but", msg);
    }
    if (app.ndocs == 0)
        cmd_new();
}

/* ------------------------------------------------------------------ */
/* search                                                              */
/* ------------------------------------------------------------------ */

static int find_global = 0;             /* last Find was "in all open files" */
static int repl_all    = 0;             /* state of "Replace all occurrences" */

static void find_next(Doc *d)
{
    long s, e, from, p;
    if (!find_buf[0])
        return;
    from = doc_sel_range(d, &s, &e) ? e : d->cursor;
    p = doc_find(d, find_buf, find_case, from);
    if (p < 0)
        dlg_alert(1, "Text not found:", find_buf);
    else
        win_show(d, p, p + (long)strlen(find_buf));
}

/* Continue in the following documents (in menu order), wrapping around and
 * ending in the start of the document we began in. */
static void find_next_global(Doc *start)
{
    int n = app.ndocs, i0 = 0, i, k;
    long s, e, from, p;
    Doc *d;

    if (!find_buf[0] || n == 0)
        return;
    for (i = 0; i < n; i++)
        if (app.docs[i] == start)
            i0 = i;
    for (k = 0; k <= n; k++) {
        d = app.docs[(i0 + k) % n];
        from = 0;
        if (k == 0)
            from = doc_sel_range(d, &s, &e) ? e : d->cursor;
        p = doc_find_nowrap(d, find_buf, find_case, from);
        if (p >= 0) {
            win_set_top(d);
            win_show(d, p, p + (long)strlen(find_buf));
            return;
        }
    }
    dlg_alert(1, "Not found in open files:", find_buf);
}

static void cmd_find(Doc *d, int global)
{
    int all = 0;
    if (dlg_search(global ? "Find in all open files" : "Find", 0,
                   find_buf, repl_buf, &find_case, &all) == DLG_FIND) {
        find_global = global;
        if (global)
            find_next_global(d);
        else
            find_next(d);
    }
}

static void report_replaced(long n, int files)
{
    char a[40], b[40];
    if (n <= 0) {
        dlg_alert(1, "Text not found:", find_buf);
        return;
    }
    sprintf(a, "%ld replacement(s) made", n);
    if (files > 0)
        sprintf(b, "in %d file(s).", files);
    else
        b[0] = 0;
    dlg_alert(1, a, b);
}

static void cmd_replace(Doc *d)
{
    long n;
    switch (dlg_search("Find and replace", 1, find_buf, repl_buf,
                       &find_case, &repl_all)) {
    case DLG_REPLACE:
        find_global = 0;
        if (repl_all) {
            n = doc_replace_all(d, find_buf, repl_buf, find_case);
            win_refresh(d);
            report_replaced(n, 0);
        } else {
            if (doc_replace_sel(d, find_buf, repl_buf, find_case))
                win_refresh(d);
            find_next(d);
        }
        break;
    case DLG_FINDNEXT:
        find_global = 0;
        find_next(d);
        break;
    }
}

static void cmd_replace_files(void)
{
    long n, total = 0;
    int i, files = 0, dummy = 0;
    Doc *d;

    if (dlg_search("Replace in all open files", 2, find_buf, repl_buf,
                   &find_case, &dummy) != DLG_REPLACEALL || !find_buf[0])
        return;
    for (i = 0; i < app.ndocs; i++) {
        d = app.docs[i];
        n = doc_replace_all(d, find_buf, repl_buf, find_case);
        if (n > 0) {
            total += n;
            files++;
            win_refresh(d);
        }
        cmd_check_error(d);
    }
    report_replaced(total, files);
}

/* ------------------------------------------------------------------ */
/* bookmarks                                                           */
/* ------------------------------------------------------------------ */

static void bm_goto(int slot)
{
    Doc *d = bm_tab[slot].doc;
    char msg[24];

    if (!d) {
        sprintf(msg, "F%d is not set.", slot + 1);
        dlg_alert(1, "No bookmark:", msg);
        return;
    }
    win_set_top(d);
    win_show(d, bm_tab[slot].pos, bm_tab[slot].pos);
}

static void bm_set_here(int slot, Doc *d)
{
    bm_set(slot, d, tb_line_start(&d->tb, tb_line_of(&d->tb, d->cursor)));
}

/* ------------------------------------------------------------------ */
/* dispatcher                                                          */
/* ------------------------------------------------------------------ */

static void next_window(Doc *top)
{
    int i;
    if (app.ndocs < 2)
        return;
    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i] == top)
            break;
    win_set_top(app.docs[(i + 1) % app.ndocs]);
}

void cmd_execute(int cmd)
{
    Doc *d = app_top_doc();
    long len;

    switch (cmd) {
    case CMD_ABOUT:
        form_alert(1, "[1][GEM Text Editor 1.0|Vibe coded using Claude.|"
                      "Repository at:|"
                      "github.com/hildenborg/gemedit][OK]");
        return;
    case CMD_NEW:       cmd_new();  return;
    case CMD_OPEN:      cmd_open(); return;
    case CMD_QUIT:      cmd_quit(); return;
    case CMD_SESLOAD:   cmd_load_session(); return;
    case CMD_SESSAVE:   if (app.ndocs) cmd_save_session(); return;
    case CMD_NEXTWIN:   next_window(d); return;
    case CMD_SAVEALL:   cmd_save_all(); return;
    case CMD_REPLFILES: if (app.ndocs) cmd_replace_files(); return;
    case CMD_BMCLEAR:   bm_clear_all(); return;
    }
    if (cmd >= CMD_WIN0 && cmd < CMD_WIN0 + MAX_DOCS) {
        if (cmd - CMD_WIN0 < app.ndocs)
            win_set_top(app.docs[cmd - CMD_WIN0]);
        return;
    }
    if (cmd >= CMD_BMGO0 && cmd < CMD_BMGO0 + BM_MAX) {
        bm_goto(cmd - CMD_BMGO0);
        return;
    }
    if (!d)
        return;
    if (cmd >= CMD_BMSET0 && cmd < CMD_BMSET0 + BM_MAX) {
        bm_set_here(cmd - CMD_BMSET0, d);
        return;
    }

    switch (cmd) {
    case CMD_CLOSE:   cmd_close_doc(d); return;
    case CMD_SAVE:    cmd_save(d);      return;
    case CMD_SAVEAS:  cmd_saveas(d);    return;
    case CMD_UNDO:    if (doc_undo(d)) win_refresh(d); break;
    case CMD_REDO:    if (doc_redo(d)) win_refresh(d); break;
    case CMD_CUT:     if (doc_cut(d))  win_refresh(d); break;
    case CMD_COPY:    doc_copy(d);      break;
    case CMD_PASTE:   if (doc_paste(d)) win_refresh(d); break;
    case CMD_SELALL:
        len = tb_len(&d->tb);
        doc_select(d, 0L, len);
        win_refresh(d);
        break;
    case CMD_FIND:     cmd_find(d, 0); break;
    case CMD_FINDALL:  cmd_find(d, 1); break;
    case CMD_FINDNEXT:
        if (!find_buf[0])
            cmd_find(d, 0);
        else if (find_global)
            find_next_global(d);
        else
            find_next(d);
        break;
    case CMD_REPLACE:  cmd_replace(d); break;
    }
    cmd_check_error(d);
}
