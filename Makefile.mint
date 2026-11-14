# Makefile.mint - build with the m68k-atari-mint toolchain + gemlib
#
#   make -f Makefile.mint                                # tools found in PATH
#   make -f Makefile.mint MINT_TOOLKIT=/opt/cross-mint  # tools in $(MINT_TOOLKIT)/bin
#   make -f Makefile.mint clean
#
# Objects go to build-mint/ so they never mix with the m68k-atari-elf build.

.DEFAULT_GOAL := all

# Project settings
BUILD_DIR   := build-mint
TARGET_NAME ?= gedit
CPU         := 68000
MINT_TOOLKIT ?=

CFLAGS  := -Wall -Os -m$(CPU) -DNO_FSEL_EXINPUT
LDFLAGS := -m$(CPU) -s
LIBS    := -lgem

SOURCES := main.c app.c commands.c menu.c dialog.c window.c doc.c textbuf.c undo.c clip.c bookmark.c session.c

# Toolchain
CROSS := $(if $(MINT_TOOLKIT),$(MINT_TOOLKIT)/bin/)m68k-atari-mint-
CC    := $(CROSS)gcc

OBJS       := $(foreach source,$(SOURCES),$(BUILD_DIR)/$(basename $(source)).o)
TARGET_PRG := $(BUILD_DIR)/$(TARGET_NAME).prg

all: $(TARGET_PRG)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# mint-gcc links straight to a TOS executable
$(TARGET_PRG): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

# Header dependencies (keep in step with Makefile)
B := $(BUILD_DIR)
$(B)/main.o $(B)/app.o $(B)/commands.o $(B)/menu.o $(B)/dialog.o $(B)/window.o: gemcompat.h app.h doc.h
$(B)/doc.o: doc.h textbuf.h undo.h clip.h bookmark.h
$(B)/bookmark.o $(B)/app.o $(B)/commands.o $(B)/menu.o: bookmark.h doc.h textbuf.h undo.h
$(B)/commands.o $(B)/menu.o $(B)/main.o: commands.h
$(B)/session.o $(B)/commands.o: session.h doc.h bookmark.h

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
