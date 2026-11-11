/* app.h - global application state */
#ifndef APP_H
#define APP_H

#include "gemcompat.h"
#include "doc.h"

#define MAX_DOCS 8              /* AES gives us at most 8 windows on plain TOS */

typedef struct {
    gint    ap_id;
    gint    vh;                 /* VDI virtual workstation               */
    gint    cw, ch;             /* system font cell size                 */
    gint    scrw, scrh;
    Rect    desk;               /* desktop work area (below menu bar)    */
    OBJECT *menu;
    Doc    *docs[MAX_DOCS];
    int     ndocs;
    int     running;
    int     untitled_seq;
} App;

extern App app;

int   app_init(void);
void  app_exit(void);
void  app_desk_update(void);

Doc  *app_doc_new(void);                 /* allocates + registers, no window */
void  app_doc_remove(Doc *d);            /* unregisters + frees               */
Doc  *app_doc_from_handle(gint handle);
Doc  *app_top_doc(void);

#endif
