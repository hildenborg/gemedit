#!/bin/bash

#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# You need to set up Hatari with a tos image before calling this script.
# You also need a folder to use as a gem drive.

# This is the extension less name of your executable.
export TARGET_NAME=gedit

# Set this to where your Hatari ST gem drive is.
export ST_HD_PATH=$HOME/sthd

# Set this to where your toolchain is.
export TOOLKIT=$HOME/toolchain/m68k-atari-elf

export GDB_COM_PORT=127.0.0.1:1234

# Start VsCode
code .

