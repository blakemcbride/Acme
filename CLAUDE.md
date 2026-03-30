# Project Context

This is a **standalone acme editor** for Linux, derived from plan9port
(Plan 9 from User Space). It requires zero external Plan 9 programs —
no mk, 9c, 9l, 9ar, 9pserve, or PLAN9 environment variable.

## Repository Layout

- `src/cmd/acme/` — original acme source files
- `src/cmd/acme-sdl3/` — standalone acme build (SDL3, embedded devdraw/fontsrv/plumber)
- `src/cmd/devdraw/` — graphics backend (SDL3)
- `src/cmd/fontsrv/` — font serving (FreeType/fontconfig)
- `src/lib*/` — 18 static libraries built from source
- `src/mklib.mk` — shared GNU make include for all libraries
- `bin/` — output directory (only `acme` binary after build)
- `lib/` — built .a files and support files (keyboard)
- `include/` — Plan 9 C headers (u.h, libc.h, draw.h, etc.)
- `Makefile` — top-level GNU make orchestrator

## Build System

Uses GNU make exclusively. No Plan 9 build tools.

```
make            # build all 18 libraries + acme binary -> bin/acme
make clean      # delete .o files and bin/acme
make realclean  # clean + delete lib/*.a
```

### Structure:
- `Makefile` — top-level, builds libs then acme-sdl3
- `src/mklib.mk` — common include for all 18 library Makefiles
- `src/lib*/Makefile` — one per library, defines LIBNAME, SRCS, includes ../mklib.mk
- `src/cmd/acme-sdl3/Makefile` — builds the acme binary

### Key build details:
- `override PLAN9 :=` in top-level Makefile prevents env var override
- `-DPLAN9PORT` is set for all libraries (required by lib9/fmt/ code)
- `lib9` has three source dirs: main, fmt/, utf/ (fmt/ needs `-Ifmt`)
- `lib9/get9root.c` gets `-DPLAN9_TARGET="$(PLAN9)"` for fallback path
- `libthread` needs `EXTRA_CFLAGS = -I.` for local headers
- `libip` includes `Linux.c` for platform-specific code
- `libsec` and `libmp` have sources in `port/` subdirectory
- acme-sdl3 links against all 18 libs plus SDL3, fontconfig, freetype, z

### The 18 libraries:
lib9 libbio libregexp libthread libmux lib9pclient libauth
libauthsrv libip libndb libsec libmp libcomplete libplumb
libdraw libmemdraw libmemlayer libframe

## SDL3 Backend

Replaced X11 backend in devdraw with SDL3 for native Wayland support.

### Key design points:
- Software rendering via memdraw, texture upload at flush (like macOS backend)
- Cross-thread dispatch: rpc_flush posts custom SDL event, graphics thread renders
- rpc_resizeimg uses Rendez condition variable for synchronous dispatch
- XRGB32 maps directly to SDL_PIXELFORMAT_XRGB8888
- No PRIMARY selection (SDL3 only exposes CLIPBOARD)
- rpc_bouncemouse is a no-op (rio/9wm specific)
- fontsrv reuses x11.c via include (fontconfig/freetype, not display)

### Backend files:
- `src/cmd/devdraw/sdl3-screen.c` — SDL3 graphics backend
- `src/cmd/devdraw/sdl3-draw.c` — software memdraw ops
- `src/cmd/fontsrv/x11.c` — font rendering (fontconfig/freetype)

## Embedded Plumber

Plumbing rules are embedded in `src/cmd/acme-sdl3/plumb-glue.c` as
`defaultrules[]`. No external plumb files or Plan 9 plumber needed.
Users can still override via `~/lib/plumbing`. Default rules use
`xdg-open` for URLs, images, and PDFs.

## Stale Socket Handling

`src/lib9/post9p.c` removes stale Unix domain sockets before launching
9pserve, preventing "Address already in use" errors after crashes.

## Coding style
- Plan 9 C style: terse, minimal abstractions, tabs for indentation
- nil instead of NULL (defined as 0 in u.h)
- Functions: emalloc, smprint, sysfatal, fprint, werrstr
- Threading: libthread with QLock, Rendez, channels (CSP-style)
