# Building and debugging in VS Code with Hatari

Pressing **F5** in VS Code builds the editor, starts it in the
[Hatari forked](https://github.com/hildenborg/hatari) emulator, and attaches gdb to it, so
breakpoints in the C source work as on any native program. Stopping the
debugger closes Hatari again.

The files involved:

| File | Role |
|------|------|
| `Makefile` | Builds `build/gedit` (ELF with debug info, read by gdb) and `build/gedit.prg` (the program the ST runs). |
| `.vscode/tasks.json` | The build task, plus tasks that start and stop Hatari through `hatari_debug.sh`. |
| `.vscode/launch.json` | The `cdt debug` configuration: runs the tasks, starts `gdb-multiarch` and connects it to Hatari. |
| `hatari_debug.sh` | Copies the program to the emulated hard drive and runs Hatari with its gdb server. |
| `.vscode/c_cpp_properties.json` | IntelliSense only (include path and compiler of the toolchain); not part of the debug flow. |

## Requirements

- **m68k-atari-elf toolchain** in `~/toolchain/m68k-atari-elf` (the
  `Makefile` default; override with `make TOOLKIT=...`).
- **Hatari** with the built-in gdb server (`hatari --help` lists
  `--gdbserver`), on `PATH`.
- **TOS image** at `~/tos/tos206.img` (change with `--tos`, see below).
- **gdb-multiarch** (any gdb that knows m68k).
- **VS Code extensions**: *C/C++* (`ms-vscode.cpptools`) for IntelliSense and
  *CDT GDB Debug Adapter* (`eclipse-cdt.cdt-gdb-vscode`), which provides the
  `gdbtarget` debug type.
- **Optional VS Code extension**: *CDT Memory Inspector*
  (`eclipse-cdt.memory-inspector`), to view and edit the ST's memory while
  the program is stopped.
- `ss` (from iproute2), used by the script to check the gdb port.

## What happens when you press F5

```
F5  ─► launch.json "cdt debug"
        │
        ├─ preLaunchTask "Start Hatari debug"
        │     ├─ dependsOn "Atari ST cross compile"  ─► make
        │     │                                         build/gedit      (ELF + debug info)
        │     │                                         build/gedit.prg  (ST program)
        │     └─ hatari_debug.sh --inferior gedit.prg --build build --harddrive sthd
        │           ├─ copy build/gedit.prg ─► sthd/gedit.prg   (= C:\GEDIT.PRG)
        │           └─ hatari --gdbserver 1234 --gdbserver-wait on --auto C:\GEDIT.PRG
        │                 TOS boots, GEDIT.PRG starts, Hatari pauses it and prints
        │                 "GDB server: program '...' started, waiting for gdb to connect"
        │                 └─ VS Code sees this line: the task is "ready"
        │
        ├─ gdb-multiarch loads build/gedit, connects to localhost:1234,
        │  sets your breakpoints and continues the program
        │
        └─ when the debug session ends:
           postDebugTask "Stop Hatari debug" ─► hatari_debug.sh --stop ─► Hatari is closed
```

### 1. Build (`Makefile`, task "Atari ST cross compile")

`make` compiles every file in `SOURCES` with `-g` and links
`build/gedit`, an ELF file that still has its debug information. Then
`m68k-atari-elf-prg` converts it to `build/gedit.prg`, the GEMDOS executable.
gdb reads the ELF file; the ST runs the `.prg`. Both come from the same link,
so their code is identical.

This task is also the default build task (**Ctrl+Shift+B**). Problems from gcc
show up in the Problems view.

### 2. Start Hatari (`hatari_debug.sh`, task "Start Hatari debug")

The task runs the script with the program name and folders of this project.
The script:

1. Copies `build/gedit.prg` to the hard drive folder `sthd/` in the project
   (created if missing, ignored by git). Hatari mounts this folder as drive
   `C:`, so the program becomes `C:\GEDIT.PRG`.
2. Refuses to start if the gdb port is already in use, which usually means a
   Hatari from an earlier session is still running.
3. Starts Hatari as an STE with 4 MB RAM and these debug options:
   - `--gdbserver 1234`: listen for gdb on TCP port 1234.
   - `--auto C:\GEDIT.PRG`: start the program once the desktop is up.
   - `--gdbserver-wait on`: pause the program at its first instruction until
     gdb connects.
   - `--symload off`: gdb has its own symbols; the `.prg` has none.
4. Saves Hatari's process ID so the stop task can find it, and stays running
   until Hatari exits.

The task is a *background* task, because Hatari keeps running. VS Code
treats it as ready when Hatari prints
`GDB server: program '...' started, waiting for gdb to connect`
(the `endsPattern` in `tasks.json`). Error lines from the script
(`hatari_debug.sh: ...`) appear in the Problems view.

**Why gdb has to wait for that line:** the ELF file is linked at address 0,
but GEMDOS loads the program wherever free memory starts. gdb asks Hatari for
the load address once, right after connecting. Only after the program has
started can Hatari answer, so gdb then shifts all symbols to where the
program really is. If gdb connected while TOS was still booting, every
breakpoint would land at the wrong address and never trigger.

### 3. Attach gdb (`launch.json`, configuration "cdt debug")

The CDT adapter starts `gdb-multiarch`, loads the symbols from
`build/gedit` (`program`), and connects with `target remote localhost:1234`
(`target`). It then sets the breakpoints from the editor and continues the
program, so it runs until your first breakpoint.

From here, stepping, variables, call stack and the Debug Console work as
usual. In the Debug Console, `>` sends a command to gdb directly, and
`>monitor <command>` runs a Hatari debugger command.

### 4. Stop (task "Stop Hatari debug")

When the debug session ends, `postDebugTask` runs
`hatari_debug.sh --stop`. It reads the saved process ID, checks that the
process is still Hatari, and kills it. The start task then ends normally.
If you close Hatari yourself, the session ends and the stop task does
nothing.

## Settings that must agree

Some values appear in more than one file. When you change one, change the
others:

| Value | Where |
|-------|-------|
| Program name `gedit` | `Makefile` (`TARGET_NAME`), `tasks.json` (`--inferior gedit.prg`), `launch.json` (`program`) |
| Build folder `build` | `Makefile` (`BUILD_DIR`), `tasks.json` (`--build`), `launch.json` (`program`) |
| gdb port `1234` | `launch.json` (`target.port`); the script's default (add `--port` to both tasks if you change it) |
| Toolchain folder | `Makefile` (`TOOLKIT`), `c_cpp_properties.json` |

## `hatari_debug.sh` options

The script also works outside VS Code, e.g. to connect gdb by hand:

```
./hatari_debug.sh --inferior <program> [--build <dir>] [--tos <file>]
                  [--harddrive <dir>] [--port <port>]
./hatari_debug.sh --stop [--port <port>]
```

| Option | Default | Meaning |
|--------|---------|---------|
| `--inferior` | (required) | Program to debug, relative to the build folder. It is copied to the same path on the hard drive. |
| `--build` | `./build` | Folder holding the built program. |
| `--harddrive` | `~/sthd` (the task passes `sthd/` in the project) | Folder Hatari mounts as `C:`. |
| `--tos` | `~/tos/tos206.img` | TOS image. To use another TOS for debugging, add `--tos` to the args of "Start Hatari debug". |
| `--port` | `1234` | gdb server TCP port. |
| `--stop` | | Close the Hatari started on `--port`. |

Manual session:

```
./hatari_debug.sh --inferior gedit.prg --harddrive sthd
# in another terminal, after "waiting for gdb to connect":
gdb-multiarch build/gedit -ex 'target remote localhost:1234'
```

## Troubleshooting

- **Breakpoint stays grey (unverified) and never triggers.** Hover over it
  for gdb's reason. "No source file named ..." means the debug info in
  `build/gedit` points to another folder, e.g. when `build/` was copied from
  another checkout. `make` will not rebuild because the files look up to
  date; run `make clean && make`.
- **"port 1234 is already in use (is Hatari still running?)".** A Hatari from
  an earlier session is still open. Close it, or run
  `./hatari_debug.sh --stop`.
- **F5 waits forever for the task.** Hatari never printed the
  "waiting for gdb to connect" line: check the task's terminal. Usual causes
  are a missing TOS image, a TOS that does not boot to the desktop, or the
  program not being started from `C:`.
- **"hatari exited"** right away: check the Hatari messages in the task
  terminal (wrong TOS image, bad option for this Hatari version).
