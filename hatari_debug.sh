#!/bin/bash

#	Copyright (C) 2025 Mikael Hildenborg
#	SPDX-License-Identifier: MIT

# Copy a program from the build folder to the GEMDOS hard drive, then start
# Hatari with its built-in gdb server and autostart that program, so gdb can
# attach to it on 127.0.0.1:<port>.
#
# Usage: hatari_debug.sh --inferior <program> [--build <dir>] [--tos <file>]
#                        [--harddrive <dir>] [--port <port>]
#        hatari_debug.sh --stop [--port <port>]
#
#   --build <dir>       folder holding the built program (default ./build)
#   --tos <file>        TOS image                  (default ~/tos/tos206.img)
#   --harddrive <dir>   folder used as drive C:    (default ~/sthd)
#   --port <port>       gdb server TCP port        (default 1234)
#   --inferior <prog>   program to debug, required. Path relative to the build
#                       folder (e.g. gedit.prg); it is copied to the same
#                       relative path in the hard drive folder.
#   --stop              kill the Hatari started by this script on --port
#                       (used as VS Code's postDebugTask).

usage()
{
	sed -n '10,23s/^# \{0,1\}//p' "$0"
	exit "${1:-1}"
}

die()
{
	echo "$(basename "$0"): $*" >&2
	exit 1
}

BUILD_DIR="build"
TOS_IMG="$HOME/tos/tos206.img"
HD_DIR="$HOME/sthd"
PORT=1234
INFERIOR=""
STOP=0

while [ $# -gt 0 ]; do
	case "$1" in
		--build|--tos|--harddrive|--port|--inferior)
			[ $# -ge 2 ] || die "$1 needs a value"
			opt="$1"; val="$2"; shift 2 ;;
		--build=*|--tos=*|--harddrive=*|--port=*|--inferior=*)
			opt="${1%%=*}"; val="${1#*=}"; shift ;;
		--stop)
			STOP=1; shift; continue ;;
		-h|--help)
			usage 0 ;;
		*)
			die "unknown argument '$1' (see --help)" ;;
	esac
	case "$opt" in
		--build)     BUILD_DIR="$val" ;;
		--tos)       TOS_IMG="$val" ;;
		--harddrive) HD_DIR="$val" ;;
		--port)      PORT="$val" ;;
		--inferior)  INFERIOR="$val" ;;
	esac
done

case "$PORT" in
	''|*[!0-9]*) die "port must be a number: $PORT" ;;
esac
[ "$PORT" -ge 1 ] && [ "$PORT" -le 65535 ] || die "port out of range: $PORT"

# Hatari's PID is kept here while it runs, so --stop can find it.
PID_FILE="${XDG_RUNTIME_DIR:-/tmp}/hatari_debug_$(id -u)_$PORT.pid"

if [ "$STOP" -eq 1 ]; then
	[ -f "$PID_FILE" ] || exit 0
	pid="$(cat "$PID_FILE")"
	rm -f "$PID_FILE"
	case "$pid" in
		''|*[!0-9]*) exit 0 ;;
	esac
	# Only kill the process if it is still Hatari (the PID may be reused).
	if [ "$(ps -p "$pid" -o comm= 2>/dev/null)" = "hatari" ]; then
		kill "$pid"
		echo "stopped Hatari (pid $pid)"
	fi
	exit 0
fi

[ -n "$INFERIOR" ] || die "--inferior must be set (see --help)"
[ -f "$TOS_IMG" ]  || die "TOS image not found: $TOS_IMG"
[ -d "$BUILD_DIR" ] || die "build folder not found: $BUILD_DIR"
[ -d "$HD_DIR" ]   || die "hard drive folder not found: $HD_DIR"

# The inferior is a path relative to the build folder; copy it to the same
# relative path in the hard drive folder.
case "$INFERIOR" in
	/*|../*|*/../*|..) die "inferior must be a path inside the build folder: $INFERIOR" ;;
esac
REL="${INFERIOR#./}"
SRC="$BUILD_DIR/$REL"
[ -f "$SRC" ] || die "inferior not found in build folder: $SRC"
HD_ABS="$(cd "$HD_DIR" && pwd -P)"
mkdir -p "$(dirname "$HD_ABS/$REL")" || die "cannot create folder for $REL in $HD_ABS"
cp "$SRC" "$HD_ABS/$REL" || die "cannot copy $SRC to $HD_ABS/$REL"

# Atari path on drive C:, with backslashes and upper case (GEMDOS 8.3 names).
ATARI_PATH="C:\\$(printf '%s' "$REL" | tr '/' '\\' | tr '[:lower:]' '[:upper:]')"

port_listening()
{
	ss -ltnH "sport = :$PORT" 2>/dev/null | grep -q .
}

port_listening && die "port $PORT is already in use (is Hatari still running?)"

echo "TOS:        $TOS_IMG"
echo "Hard drive: $HD_ABS (C:)"
echo "Inferior:   $SRC -> $ATARI_PATH"

hatari \
	--machine ste \
	--memsize 4 \
	--fast-boot true \
	--confirm-quit false \
	--tos "$TOS_IMG" \
	--harddrive "$HD_ABS" \
	--gemdos-drive C \
	--gdbserver "$PORT" \
	--gdbserver-wait on \
	--auto "$ATARI_PATH" &
HATARI_PID=$!
echo "$HATARI_PID" > "$PID_FILE"
trap 'kill "$HATARI_PID" 2>/dev/null' INT TERM HUP
trap 'rm -f "$PID_FILE"' EXIT

# Wait until Hatari's gdb server is listening, then announce it. VS Code's
# background task waits for the "gdb server ready" line before attaching.
tries=0
until port_listening; do
	kill -0 "$HATARI_PID" 2>/dev/null || { wait "$HATARI_PID"; die "hatari exited (status $?)"; }
	tries=$((tries + 1))
	[ "$tries" -le 100 ] || { kill "$HATARI_PID"; die "gdb server did not open port $PORT"; }
	sleep 0.1
done
echo "gdb server ready: target remote 127.0.0.1:$PORT"

wait "$HATARI_PID"
status=$?
# 143 = killed by SIGTERM, i.e. stopped with --stop: not an error.
[ "$status" -eq 143 ] && { echo "Hatari stopped"; exit 0; }
exit "$status"
