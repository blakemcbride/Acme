# Building Acme on Windows

This document covers the Windows build, which produces a single
`acme.exe` with no MSYS2 or Cygwin runtime dependency — only Windows
system DLLs (kernel32, user32, gdi32, etc.).  The binary can be
copied to any Windows 10/11 machine and run without any other files.

## Overview

The build uses an MSYS2 toolchain (UCRT64 by default) and statically
links against every dependency.  POSIX APIs that don't exist on
native Windows (`fork`, `openpty`, `socketpair`, etc.) are replaced
with Win32 equivalents (`CreateProcess`, ConPTY, named pipes).  The
fontconfig configuration is embedded in the binary, and directory
enumeration uses `FindFirstFile`/`FindNextFile` instead of POSIX
`readdir`.

The build is shell-aware: if you launch from MSYS2's MINGW64 or
CLANG64 shell, `MSYSTEM_PREFIX` is honored automatically and the
build retargets to that subsystem with no flags.  You can also force
a target on the command line:

```
make MSYS_PREFIX=/mingw64
```

## Prerequisites

MSYS2 is needed at **build time** to provide the toolchain and static
libraries.  It is **not** needed at runtime.

1. Install MSYS2 from https://www.msys2.org/

2. Launch the **MSYS2 UCRT64** shell from the Start menu (the icon
   labeled "MSYS2 UCRT64").  Avoid the plain "MSYS2 MSYS" shell — it
   uses the Cygwin-derived MSYS gcc, which is not what this build
   targets.

3. Install dependencies:

```
pacman -Syu
pacman -S git make
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-sdl3 \
          mingw-w64-ucrt-x86_64-fontconfig \
          mingw-w64-ucrt-x86_64-freetype
```

The `toolchain` group already pulls in `pkgconf`, which provides the
`pkg-config` binary.  Don't install `mingw-w64-ucrt-x86_64-pkg-config`
explicitly — it conflicts with `pkgconf` and pacman will refuse the
transaction.

If you launched from the MINGW64 shell instead of UCRT64, swap
`mingw-w64-ucrt-x86_64-*` for `mingw-w64-x86_64-*`.  CLANG64 uses
`mingw-w64-clang-x86_64-*`.

If a package name has changed, search with `pacman -Ss <name>` and
adjust accordingly.  Note that the SDL3 package is lowercase
(`sdl3`), unlike SDL2.

## A note on `PATH`

Several common Windows installs (Strawberry Perl, Anaconda, Git for
Windows) ship their own `gcc`/`pkg-config` and place them on `PATH`
ahead of MSYS2.  Symptoms include `pkg-config` failing with
`Can't locate Pod/Usage.pm`, the build silently picking up the wrong
compiler, or `pkg-config --cflags freetype2` returning nothing — which
manifests as `fatal error: ft2build.h: No such file or directory`.

If `which gcc pkg-config` does not resolve under your active subsystem
(`/ucrt64/bin/...`), prepend it for the build:

```
export PATH="/ucrt64/bin:/usr/bin:$PATH"
```

(Substitute `/mingw64/bin` or `/clang64/bin` if you're not on UCRT64.)

## Building

From the repository root:

```
make
```

The Makefile auto-detects the active subsystem via `MSYSTEM_PREFIX`,
so no environment variables need to be set in the standard case.  On
Windows, `make` always produces the static, standalone build — there
is no separate "native" target to invoke.

The resulting binary is `bin/acme.exe`, approximately 13 MB, and
depends only on Windows system DLLs.

## Distributing

Copy `bin/acme.exe` to any location on any Windows 10/11 machine
and run it.  No installation, no configuration, no other files
required.  The fontconfig configuration and character tables are
embedded in the binary.

## Running

Double-click `acme.exe` or run from a command prompt:

```
acme.exe
```

The default font is Consolas (included with Windows).  Fontconfig
uses an embedded configuration that indexes `C:\Windows\Fonts`.

## Mouse

Acme requires a three-button mouse.  The following options are
available:

**Modifier-click emulation (built in):**
- Ctrl-click = middle button (button 2) — execute
- Alt-click = right button (button 3) — acquire/look

**Three-button mouse:**
Any standard USB or Bluetooth three-button mouse works natively.
This is strongly recommended for extended use.  Acme's mouse-driven
interface is far more comfortable with real buttons.

**Scroll wheel:**
Scroll up/down maps to buttons 4 and 5 (same as Linux).

## Win Command (Interactive Shell)

The `Win` command opens an interactive shell window, spawning
`cmd.exe` via Windows ConPTY (`CreatePseudoConsole`, available on
Windows 10 1809+).

Type `Win` in any tag bar and middle-click to execute.  A new
window opens with `cmd.exe` running.  Type commands followed by
Enter to execute them.  Output appears inline.

The shell honors the `COMSPEC` environment variable; by default
this is `C:\Windows\System32\cmd.exe`.  PowerShell can be used by
setting `COMSPEC` before launching acme.

## Plumbing

The built-in default plumbing rules dispatch URLs and recognized
file types through `explorer.exe`, which is a thin wrapper around
`ShellExecute` — so HTTP(S) links open in the user's default
browser and files open with their registered associations.  Drop
a custom `~/lib/plumbing` to override the defaults.

## Technical Notes

- The build uses a single compiler — `$(MSYS_PREFIX)/bin/gcc` — for
  every object.  `MSYS_PREFIX` defaults to `/ucrt64` and falls back
  through `MSYSTEM_PREFIX` when launched from MINGW64 or CLANG64.
  Override with `make MSYS_PREFIX=/mingw64` if needed.

- POSIX APIs replaced with Win32:
  - `fork`/`exec` → `CreateProcess`
  - `socketpair` → `_pipe`
  - `openpty` → ConPTY (`CreatePseudoConsole`)
  - `wait4` → `WaitForSingleObject` + `GetExitCodeProcess`
  - `sigaction` → minimal `signal()` with SIGINT/SIGTERM only
  - `readdir`/`fstatat` → `FindFirstFile`/`FindNextFile`
  - `getuid`/`getpwuid` → `GetUserNameA`
  - `open()` on directory → `CreateFile(FILE_FLAG_BACKUP_SEMANTICS)`

- Fontconfig configuration is compiled into the binary as an XML
  string and loaded via `FcConfigParseAndLoadFromMemory`.  No
  external `fonts.conf` file needed; fonts are discovered from
  `C:\Windows\Fonts`.

- Clipboard uses SDL3's clipboard API (Ctrl+C/Ctrl+V in other apps,
  `Snarf`/`Paste` commands within acme).

- Network interface discovery (`myipaddr`, `myetheraddr`) is stubbed
  out.  This does not affect normal editor operation.
