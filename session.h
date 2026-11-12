/* session.h - list of open documents and bookmarks, saved to a text file.
 * No GEM calls; the command layer fills / applies the Session struct.
 *
 * File format (CR/LF lines, LF also accepted when reading):
 *     GEMEDIT SESSION 1
 *     D <line> <col> <path>        one per document, in window-menu order
 *     T <doc>                      document on top (0-based index)
 *     B <slot> <doc> <line>        bookmark F<slot>, slot 1..10
 * Lines and columns are 1-based, columns count characters in the line. */
#ifndef SESSION_H
#define SESSION_H

#include "doc.h"
#include "bookmark.h"

#define SES_MAXDOCS 16

typedef struct {
    char path[PATHLEN];
    long line, col;             /* cursor, 0-based */
} SesDoc;

typedef struct {
    int    ndocs;
    SesDoc doc[SES_MAXDOCS];
    int    top;                 /* index into doc[], -1 = none           */
    int    bm_doc[BM_MAX];      /* index into doc[], -1 = slot unused    */
    long   bm_line[BM_MAX];     /* 0-based                               */
} Session;

void ses_clear(Session *s);
int  ses_write(const char *path, const Session *s);  /* 0 ok, -1 write error */
int  ses_read(const char *path, Session *s);         /* 0 ok, -1 read error, -2 not a session file */

#endif
