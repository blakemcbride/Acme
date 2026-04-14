# Building Acme as a Standalone Windows Executable

This document covers the native Windows build, which produces a
single `acme.exe` with no MSYS2 or Cygwin runtime dependency — only
Windows system DLLs (kernel32, user32, gdi32, etc.).  The binary
can be copied to any Windows 10/11 machine and run without any
other files.

For the MSYS2-based build (easier, requires MSYS2 at runtime), see
`Windows-MSYS.md` instead.

## Overview

The native build uses mingw64 gcc exclusively and statically links
against all dependencies.  POSIX APIs that don't exist on native
Windows (fork, openpty, socketpair, etc.) are replaced with Win32
equivalents (CreateProcess, ConPTY, named pipes).  The fontconfig
configuration is embedded in the binary, and directory enumeration
uses `FindFirstFile`/`FindNextFile` instead of POSIX readdir.

## Prerequisites

MSYS2 is still needed at **build time** to provide the mingw64
toolchain and static libraries.  It is **not** needed at runtime.

1. Install MSYS2 from https://www.msys2.org/

2. Launch an MSYS2 shell from the Start menu (any variant works).

3. Install dependencies:

```
pacman -Syu
pacman -S git make pkg-config
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-sdl3 \
          mingw-w64-x86_64-fontconfig \
          mingw-w64-x86_64-freetype
```

If any package names have changed, search with `pacman -Ss <name>`.

## Building

```
make native-win
```

This uses `/mingw64/bin/gcc` for all compilation and statically
links against all C, graphics, and Windows system libraries.

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

The `Win` command opens an interactive shell window.  On the native
build it spawns `cmd.exe` via Windows ConPTY (`CreatePseudoConsole`,
available on Windows 10 1809+).

Type `Win` in any tag bar and middle-click to execute.  A new
window opens with `cmd.exe` running.  Type commands followed by
Enter to execute them.  Output appears inline.

The shell honors the `COMSPEC` environment variable; by default
this is `C:\Windows\System32\cmd.exe`.  PowerShell can be used by
setting `COMSPEC` before launching acme.

## Known Limitations

- **Plumbing** — URL/file opening uses Windows' `ShellExecute` (the
  default "open" action) rather than the MSYS2 build's `cygstart`.

## Technical Notes

- Uses mingw64 gcc exclusively, statically linked.  The FreeType
  ABI shim needed by the MSYS2 build is not required here because
  the compiler and the FreeType library use the same LLP64 ABI.

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
