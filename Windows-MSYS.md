# Building Acme on Windows (MSYS2 build)

This document covers the MSYS2-based build of acme.  The resulting
binary requires MSYS2 at runtime (for the POSIX compatibility layer).
If you want a standalone executable with no MSYS2 runtime dependency,
see `Windows-native.md` instead.

## Overview

Acme's Plan 9 codebase depends on POSIX APIs (fork, exec, pipes,
PTY).  MSYS2 provides these through its Cygwin-derived runtime
(`msys-2.0.dll`), so the code compiles with few changes.  The
graphics libraries (SDL3, FreeType, fontconfig) come from MSYS2's
mingw64 packages.

## Prerequisites

1. Install MSYS2 from https://www.msys2.org/

2. Launch an **MSYS2** shell from the Start menu (MSYS, MINGW64, or
   UCRT64 all work — the build system pins the correct compilers
   automatically).

3. Install dependencies:

```
pacman -Syu
pacman -S git gcc make pkg-config zlib-devel
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-sdl3 \
          mingw-w64-x86_64-fontconfig \
          mingw-w64-x86_64-freetype
```

Two compilers are needed: the MSYS gcc (`gcc` package) compiles the
main binary with POSIX support, while the mingw64 gcc
(`mingw-w64-x86_64-gcc`) compiles a small FreeType ABI compatibility
shim.  The SDL3, fontconfig, and freetype packages provide the
prebuilt mingw64 native-Windows libraries.

If any package names have changed, search with `pacman -Ss <name>`.

## Building

```
export PKG_CONFIG_PATH=/mingw64/lib/pkgconfig
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
make
```

The `PKG_CONFIG_ALLOW_SYSTEM_*` overrides are needed because the
mingw64 `.pc` files declare `-I/mingw64/include` and
`-L/mingw64/lib`, which pkg-config normally strips as "system"
paths.  Without these overrides the SDL3 headers and libraries are
not found.

The binary is placed in `bin/acme`.

## Running

```
./bin/acme
```

The binary depends on the MSYS2 runtime (`msys-2.0.dll` and related
libraries) and mingw64 DLLs (SDL3, freetype, fontconfig).  It must
be run from an MSYS2 environment or with the MSYS2 and mingw64 DLLs
in the PATH.

The default font is Consolas (a monospace font included with
Windows).  Fonts are discovered via fontconfig, which indexes the
fonts installed in `C:\Windows\Fonts`.

Custom plumbing rules can be placed in `~/lib/plumbing` to override
the built-in defaults.  The default rules use `cygstart` (provided
by MSYS2) to open URLs, images, and PDFs with their default Windows
handlers.

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

## Technical Notes

- The build uses two compilers: MSYS gcc (`/usr/bin/gcc`, Cygwin
  ABI) for the main binary, and mingw64 gcc (`/mingw64/bin/gcc`,
  native Windows ABI) for `ft-shim.o`.  The FreeType shim is needed
  because MSYS gcc uses LP64 (`sizeof(long)` = 8) while the mingw64
  FreeType DLL uses LLP64 (`sizeof(long)` = 4), causing struct
  layout mismatches.  The shim wraps FreeType struct access behind
  an ABI-safe interface using only `int` and pointer types.

- SDL3 and fontconfig are ABI-safe (no `long` in public struct
  layouts), so they link directly without a shim.

- Network interface discovery (`myipaddr`, `myetheraddr`) is stubbed
  out on Windows.  This does not affect normal editor operation.

- Clipboard uses SDL3's clipboard API (Ctrl+C/Ctrl+V in other apps,
  `Snarf`/`Paste` commands within acme).

- The Win command spawns a shell via PTY, using the MSYS2 POSIX
  layer's `openpty()`.  It uses `$SHELL` (typically `/bin/bash`
  in MSYS2).

- If `cygstart` is not installed, install it with
  `pacman -S cygutils-extra` or override with custom plumbing rules
  in `~/lib/plumbing`.
