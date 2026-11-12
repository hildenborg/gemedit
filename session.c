/* session.c - read / write the session file (see session.h) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "session.h"

#define SES_MAGIC "GEMEDIT SESSION 1"

void ses_clear(Session *s)
{
    int i;
    memset(s, 0, sizeof *s);
    s->top = -1;
    for (i = 0; i < BM_MAX; i++)
        s->bm_doc[i] = -1;
}

int ses_write(const char *path, const Session *s)
{
    FILE *f = fopen(path, "wb");
    int i, ok;

    if (!f)
        return -1;
    fprintf(f, "%s\r\n", SES_MAGIC);
    for (i = 0; i < s->ndocs; i++)
        fprintf(f, "D %ld %ld %s\r\n", s->doc[i].line + 1, s->doc[i].col + 1,
                s->doc[i].path);
    if (s->top >= 0)
        fprintf(f, "T %d\r\n", s->top);
    for (i = 0; i < BM_MAX; i++)
        if (s->bm_doc[i] >= 0)
            fprintf(f, "B %d %d %ld\r\n", i + 1, s->bm_doc[i], s->bm_line[i] + 1);
    ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    return ok ? 0 : -1;
}

static void chomp(char *b)
{
    size_t n = strlen(b);
    while (n > 0 && (b[n - 1] == '\n' || b[n - 1] == '\r'))
        b[--n] = 0;
}

/* Unknown or malformed lines are skipped, so later versions can add more. */
int ses_read(const char *path, Session *s)
{
    FILE *f = fopen(path, "rb");
    char buf[PATHLEN + 48], *p;
    long a, b;
    int slot, di, n;
    SesDoc *sd;

    if (!f)
        return -1;
    ses_clear(s);
    if (!fgets(buf, sizeof buf, f)) {
        fclose(f);
        return -2;
    }
    chomp(buf);
    if (strcmp(buf, SES_MAGIC) != 0) {
        fclose(f);
        return -2;
    }
    while (fgets(buf, sizeof buf, f)) {
        chomp(buf);
        switch (buf[0]) {
        case 'D':
            if (s->ndocs >= SES_MAXDOCS)
                break;
            n = 0;
            if (sscanf(buf + 1, " %ld %ld %n", &a, &b, &n) < 2 || n == 0)
                break;
            p = buf + 1 + n;
            if (!*p || strlen(p) >= PATHLEN)
                break;
            sd = &s->doc[s->ndocs++];
            strcpy(sd->path, p);
            sd->line = a > 0 ? a - 1 : 0;
            sd->col = b > 0 ? b - 1 : 0;
            break;
        case 'T':
            if (sscanf(buf + 1, " %d", &di) == 1)
                s->top = di;
            break;
        case 'B':
            if (sscanf(buf + 1, " %d %d %ld", &slot, &di, &a) == 3 &&
                slot >= 1 && slot <= BM_MAX && di >= 0) {
                s->bm_doc[slot - 1] = di;
                s->bm_line[slot - 1] = a > 0 ? a - 1 : 0;
            }
            break;
        }
    }
    fclose(f);
    if (s->top >= s->ndocs)
        s->top = -1;
    for (slot = 0; slot < BM_MAX; slot++)
        if (s->bm_doc[slot] >= s->ndocs)
            s->bm_doc[slot] = -1;
    return 0;
}
