/* clip.h - internal clipboard shared by all documents */
#ifndef CLIP_H
#define CLIP_H
int         clip_set(const char *s, long n);   /* 0 = ok */
const char *clip_get(void);
long        clip_len(void);
#endif
