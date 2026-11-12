.DEFAULT_GOAL := all

# Project settings
BUILD_DIR := build
TARGET_NAME ?= gemedit
CPU := 68000
TOOLKIT	?= /usr/local/bin/m68k-atari-elf

LDFLAGS :=
CFLAGS := -Wall -Os -g -m$(CPU) -DNO_FSEL_EXINPUT
ASFLAGS := -g --register-prefix-optional -mcpu=$(CPU)

SOURCES := main.c app.c commands.c menu.c dialog.c window.c doc.c textbuf.c undo.c clip.c bookmark.c session.c

# Toolchain command settings
# Project build architecture settings
TOOLKIT_BIN := $(TOOLKIT)/bin
CC := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
AS := $(TOOLKIT_BIN)/m68k-atari-elf-as
LD := $(TOOLKIT_BIN)/m68k-atari-elf-gcc
ELFTOPRG := $(TOOLKIT_BIN)/m68k-atari-elf-prg

# Building object list
OBJS := $(foreach source,$(SOURCES),$(BUILD_DIR)/$(basename $(source)).o)

# Targets
TARGET_PRG := $(BUILD_DIR)/$(TARGET_NAME).prg
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME)

# asm
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) -c $< -o $@

# c
$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build target
$(TARGET_PRG): $(TARGET_ELF)
	$(ELFTOPRG) $< $@

$(TARGET_ELF): $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@ -ltos -lvdi -laes

# Create build directory
$(BUILD_DIR): $(shell mkdir -p $(BUILD_DIR))

all:	$(BUILD_DIR) $(TARGET_PRG)

# Header dependencies
B := $(BUILD_DIR)
$(B)/main.o $(B)/app.o $(B)/commands.o $(B)/menu.o $(B)/dialog.o $(B)/window.o: gemcompat.h app.h doc.h
$(B)/doc.o: doc.h textbuf.h undo.h clip.h bookmark.h
$(B)/bookmark.o $(B)/app.o $(B)/commands.o $(B)/menu.o: bookmark.h doc.h textbuf.h undo.h
$(B)/commands.o $(B)/menu.o $(B)/main.o: commands.h
$(B)/session.o $(B)/commands.o: session.h doc.h bookmark.h

clean:
	$(shell rm -r $(BUILD_DIR))