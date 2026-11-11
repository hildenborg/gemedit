/* app.c - AES/VDI start-up and the list of open documents */
#include "gemcompat.h"
#include "app.h"
#include "bookmark.h"

App app;

int rect_intersect(const Rect *a, const Rect *b, Rect *r)
{
    gint x1 = a->x > b->x ? a->x : b->x;
    gint y1 = a->y > b->y ? a->y : b->y;
    gint x2 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
    gint y2 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
    r->x = x1;
    r->y = y1;
    r->w = x2 - x1;
    r->h = y2 - y1;
    return x2 > x1 && y2 > y1;
}

int app_init(void)
{
    gint work_in[11], work_out[57], i, dummy, h, a, b;

    memset(&app, 0, sizeof app);
    app.ap_id = appl_init();
    if (app.ap_id < 0)
        return -1;

    h = graf_handle(&app.cw, &app.ch, &dummy, &dummy);
    for (i = 0; i < 10; i++)
        work_in[i] = 1;
    work_in[10] = 2;                        /* raster coordinates */
    v_opnvwk(work_in, &h, work_out);
    if (h == 0) {
        appl_exit();
        return -1;
    }
    app.vh = h;
    app.scrw = work_out[0] + 1;
    app.scrh = work_out[1] + 1;

    vst_alignment(app.vh, 0, 5, &a, &b);    /* left / top aligned text */
    vst_color(app.vh, 1);
    vst_effects(app.vh, 0);
    graf_mouse(ARROW, 0);

    app.running = 1;
    return 0;
}

void app_exit(void)
{
    if (app.menu)
        menu_bar(app.menu, 0);
    v_clsvwk(app.vh);
    appl_exit();
}

void app_desk_update(void)
{
    wind_get(0, WF_WORKXYWH, &app.desk.x, &app.desk.y, &app.desk.w, &app.desk.h);
}

Doc *app_doc_new(void)
{
    Doc *d;
    if (app.ndocs >= MAX_DOCS)
        return 0;
    d = doc_new();
    if (!d)
        return 0;
    app.docs[app.ndocs++] = d;
    return d;
}

void app_doc_remove(Doc *d)
{
    int i;
    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i] == d)
            break;
    if (i == app.ndocs)
        return;
    for (; i + 1 < app.ndocs; i++)
        app.docs[i] = app.docs[i + 1];
    app.ndocs--;
    bm_remove_doc(d);
    doc_free(d);
}

Doc *app_doc_from_handle(gint handle)
{
    int i;
    if (handle <= 0)
        return 0;
    for (i = 0; i < app.ndocs; i++)
        if (app.docs[i]->whandle == handle)
            return app.docs[i];
    return 0;
}

Doc *app_top_doc(void)
{
    gint h, a, b, c;
    wind_get(0, WF_TOP, &h, &a, &b, &c);
    return app_doc_from_handle(h);
}
