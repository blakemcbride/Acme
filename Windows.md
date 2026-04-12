# Building Acme on Windows

Acme on Windows requires MSYS2, which provides the POSIX compatibility
layer (fork, exec, pipes, PTY) that the codebase depends on.

## Prerequisites

1. Install MSYS2 from https://www.msys2.org/

2. Open the **MSYS2 MSYS** shell (not MINGW64/UCRT64 -- the MSYS
   shell provides the POSIX runtime needed for fork/exec/pipe/PTY).

3. Install dependencies:

```
pacman -Syu
pacman -S git gcc make pkg-config
pacman -S libSDL3-devel
pacman -S libfontconfig-devel
pacman -S libfreetype-devel
pacman -S zlib-devel
```

If any package names have changed, search with `pacman -Ss <name>`.

## Building

From the MSYS2 MSYS shell:

```
make
```

The binary is placed in `bin/acme`.

## Running

Run from the MSYS2 shell:

```
./bin/acme
```

The binary depends on the MSYS2 runtime (`msys-2.0.dll` and related
libraries). It must be run from an MSYS2 environment or with the
MSYS2 DLLs in the PATH.

Fonts are discovered via fontconfig/freetype. The default plumbing
rules use `cygstart` (provided by MSYS2) to open URLs, images, and
PDFs with their default Windows handlers.

Custom plumbing rules can be placed in `~/lib/plumbing` to override
the built-in defaults.

## Mouse

Acme requires a three-button mouse. The following options are
available:

**Modifier-click emulation (built in):**
- Ctrl-click = middle button (button 2) -- execute
- Alt-click = right button (button 3) -- acquire/look

**Three-button mouse:**
Any standard USB or Bluetooth three-button mouse works natively.
This is strongly recommended for extended use. Acme's mouse-driven
interface is far more comfortable with real buttons.

**Scroll wheel:**
Scroll up/down maps to buttons 4 and 5 (same as Linux).

## Notes

- Network interface discovery (`myipaddr`, `myetheraddr`) is stubbed
  out on Windows. This does not affect normal editor operation.
- The build uses `pkg-config` to locate SDL3, freetype, and fontconfig
  headers and libraries.
- Clipboard uses SDL3's clipboard API (Ctrl+C/Ctrl+V in other apps,
  `Snarf`/`Paste` commands within acme).
- The Win command spawns a shell via PTY, using the MSYS2 POSIX
  layer's `openpty()`. It uses `$SHELL` (typically `/bin/bash`
  in MSYS2).
- If `cygstart` is not installed, install it with
  `pacman -S cygutils-extra` or override with custom plumbing rules
  in `~/lib/plumbing`.
