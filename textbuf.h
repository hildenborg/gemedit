/* textbuf.h - gap buffer plus an always-valid line start table */
#ifndef TEXTBUF_H
#define TEXTBUF_H

typedef struct {
    char *buf;              /* gap buffer storage                    */
    long  cap;              /* allocated size                        */
    long  gs, ge;           /* gap is [gs, ge)                       */
    long *lines;            /* lines[i] = offset of first char of i  */
    long  nlines;           /* always >= 1                           */
    long  lcap;
} TextBuf;

int         tb_init(TextBuf *t);
void        tb_free(TextBuf *t);
long        tb_len(const TextBuf *t);
int         tb_at(const TextBuf *t, long pos);
int         tb_insert(TextBuf *t, long pos, const char *s, long n);  /* 0 = ok */
void        tb_delete(TextBuf *t, long pos, long n);
void        tb_copy(const TextBuf *t, long pos, long n, char *dst);
const char *tb_flat(TextBuf *t);        /* contiguous text, valid until next edit */

long        tb_line_count(const TextBuf *t);
long        tb_line_start(const TextBuf *t, long line);
long        tb_line_end(const TextBuf *t, long line);   /* offset of '\n' or len */
long        tb_line_of(const TextBuf *t, long pos);

#endif
