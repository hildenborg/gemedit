# GEM Text Editor - build with the FreeMiNT cross toolchain
#   (m68k-atari-mint-gcc + gemlib).  Run `make`, copy GEMEDIT.PRG to your ST.
#
# Plain TOS 1.00 (no fsel_exinput): add -DNO_FSEL_EXINPUT to CFLAGS.

CC      = m68k-atari-mint-gcc
CFLAGS  = -O2 -Wall -m68000 -fomit-frame-pointer
LDLIBS  = -lgem

OBJS = main.o app.o commands.o menu.o dialog.o window.o doc.o textbuf.o undo.o clip.o bookmark.o

gemedit.prg: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDLIBS)
	m68k-atari-mint-strip -s $@ || true

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o app.o commands.o menu.o dialog.o window.o: gemcompat.h app.h doc.h
doc.o: doc.h textbuf.h undo.h clip.h bookmark.h
bookmark.o app.o commands.o menu.o: bookmark.h
commands.o menu.o main.o: commands.h

clean:
	rm -f *.o gemedit.prg
