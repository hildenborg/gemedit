/* gemcompat.h - one place for compiler/binding differences.
 *
 * Primary target : m68k-atari-mint-gcc + gemlib   (-lgem)
 * Also supported : Pure C (define nothing, __PUREC__ is set by the compiler)
 */
#ifndef GEMCOMPAT_H
#define GEMCOMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __PUREC__
#include <aes.h>
#include <vdi.h>
#include <tos.h>
typedef int gint;               /* 16 bit int used for all AES/VDI values */
#else
#include <gem.h>
#include <osbind.h>
typedef short gint;
#endif

/* Use fsel_exinput() (TOS 1.04+). Define NO_FSEL_EXINPUT for plain TOS 1.0. */
#ifndef NO_FSEL_EXINPUT
#define HAVE_FSEL_EXINPUT 1
#endif

/* OBJECT.ob_spec accessors - the only place that touches the union */
#define OB_SPEC_LONG(o, v)  ((o)->ob_spec.index = (long)(v))
#define OB_SPEC_STR(o, s)   ((o)->ob_spec.free_string = (s))
#define OB_SPEC_TED(o, t)   ((o)->ob_spec.tedinfo = (t))

/* keyboard state bits from evnt_multi */
#define KS_SHIFT 0x03
#define KS_CTRL  0x04
#define KS_ALT   0x08

typedef struct { gint x, y, w, h; } Rect;

int rect_intersect(const Rect *a, const Rect *b, Rect *r);

#endif
