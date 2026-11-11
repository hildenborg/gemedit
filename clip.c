#include <stdlib.h>
#include <string.h>
#include "clip.h"

static char *g_clip = 0;
static long  g_len  = 0;

int clip_set(const char *s, long n)
{
    char *p = (char *)malloc(n > 0 ? n : 1);
    if (!p)
        return -1;
    memcpy(p, s, n);
    free(g_clip);
    g_clip = p;
    g_len = n;
    return 0;
}

const char *clip_get(void) { return g_clip; }
long        clip_len(void) { return g_len; }
