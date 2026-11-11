/* dialog.h - alerts, file selector and the find/replace dialog */
#ifndef DIALOG_H
#define DIALOG_H

#define DLG_FLD 30                     /* max chars in search fields */

enum { DLG_CANCEL = 0, DLG_FIND, DLG_REPLACE, DLG_REPLACEALL, DLG_FINDNEXT };

/* 1 = file chosen (full path in outpath, >= PATHLEN bytes), 0 = cancelled */
int  dlg_fileselect(const char *title, char *outpath, const char *defname);

void dlg_alert(int icon, const char *line1, const char *line2);
int  dlg_ask_save(const char *name);           /* 1 save, 2 ignore, 3 cancel */
int  dlg_confirm_overwrite(const char *name);  /* 1 yes, 0 no */

/* Find / replace dialog.  Buffers need DLG_FLD+1 bytes.
 *   mode 0 : find                      (Find, Cancel)
 *   mode 1 : find and replace          (Replace, Find next, Cancel; has the
 *                                       "Replace all occurrences" check box)
 *   mode 2 : replace in all open files (Replace all, Cancel)
 * Returns a DLG_xxx code.  *all receives the check box state (mode 1 only). */
int  dlg_search(const char *title, int mode, char *find, char *repl,
                int *match_case, int *all);

#endif
