/* window.h - one GEM window per document: drawing, scrolling, mouse */
#ifndef WINDOW_H
#define WINDOW_H

#include "gemcompat.h"
#include "doc.h"

int  win_open(Doc *d);                  /* 0 ok, -1 no window available */
void win_close(Doc *d);
void win_set_top(Doc *d);

void win_redraw(Doc *d, const Rect *r); /* WM_REDRAW                    */
void win_refresh(Doc *d);               /* after any edit/cursor change */
void win_layout(Doc *d);                /* after size change            */
void win_show(Doc *d, long anchor, long cursor);   /* select + make visible */

void win_arrowed(Doc *d, gint what);    /* WM_ARROWED                   */
void win_vslid(Doc *d, gint pos);       /* WM_VSLID                     */
void win_hslid(Doc *d, gint pos);       /* WM_HSLID                     */
void win_set_rect(Doc *d, gint x, gint y, gint w, gint h);  /* WM_SIZED / WM_MOVED */
void win_fulled(Doc *d);                /* WM_FULLED                    */

int  win_click(Doc *d, gint mx, gint my, gint ks, gint clicks);

#endif
