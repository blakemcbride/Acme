# Standalone Acme: Direct-Linkage with SDL3 (Option A)

## Goal

Embed devdraw's graphics backend directly into acme, eliminating the
fork/exec + pipe-based RPC protocol entirely. Direct function calls
replace serialized IPC. The result is a single self-contained binary.

## Current Architecture

Three layers with a process boundary:

1. **libdraw** (in acme's process): Provides the public `draw.h` API
   (`allocimage`, `draw`, `flushimage`, etc.). Serializes drawing commands
   as single-byte opcodes with binary arguments into `Display.buf`. On
   `flushimage`, the buffer is sent via `_displaywrdraw` -> `displayrpc`
   -> pipe write to devdraw. Event reads (`_displayrdmouse`,
   `_displayrdkbd`) are also RPC calls.

2. **Pipe + Wsysmsg RPC**: `drawclient.c` forks/execs devdraw, creates a
   pipe. Each high-level operation (init, mouse read, kbd read, snarf, draw
   data, cursor, etc.) is a `Wsysmsg` RPC. The draw data commands
   (`Twrdraw`/`Trddraw`) carry the lower-level draw protocol buffer (the
   `'b'`, `'d'`, `'v'`, `'y'`, etc. opcodes from `devdraw.c`).

3. **devdraw** (server process): `srv.c` reads Wsysmsg RPCs from the pipe.
   For `Twrdraw`, it calls `draw_datawrite` which interprets draw protocol
   opcodes and calls memdraw functions directly. The SDL3 backend
   (`sdl3-screen.c`) runs the event loop on the main/graphics thread and
   dispatches rendering via custom SDL events.

## Key Insight

There are **two** serialization layers:

1. **Outer layer** -- `Wsysmsg` RPC over pipe (mouse reads, kbd reads,
   snarf, cursor, etc.)
2. **Inner layer** -- draw protocol opcodes (`'b'`, `'d'`, `'v'`, etc.)
   packed into `Display.buf`

We eliminate the outer layer. The inner layer stays for now -- it is just
memcpy within the same process, no syscalls, negligible overhead. Eliminating
it would mean rewriting every function in libdraw, which is a separate project.

## New Architecture

### New directory: `src/cmd/acme-sdl3/`

All existing code stays untouched. Four new files:

| New File | Purpose |
|----------|---------|
| `mkfile` | Builds single `acme` binary from acme sources + devdraw objects + SDL3 |
| `drawbridge.c` | Replaces `drawclient.c` -- implements all `_display*` functions as direct calls instead of pipe RPC |
| `srv-direct.c` | Modified `srv.c` -- removes RPC dispatch, delivers keyboard/mouse events via libthread `Channel` instead of pipe matching |
| `main.c` | New `threadmain` -- starts SDL3 event loop on main thread, spawns acme logic as a separate proc |

### Drawing path (unchanged logic, no pipe)

```
acme calls draw()
  -> libdraw packs opcodes into Display.buf
  -> flushimage() calls _displaywrdraw()
  -> drawbridge.c calls draw_datawrite() directly
     (was: pipe -> srv.c -> draw_datawrite)
  -> memdraw does software rendering
  -> 'v' opcode triggers rpc_flush -> SDL_PushEvent to graphics thread
  -> SDL3 thread does SDL_UpdateTexture + SDL_RenderPresent
```

### Event path (channels replace pipe RPC)

```
SDL3 event loop -> gfx_keystroke() / gfx_mousetrack()
  -> send on Channel*
     (was: queue + matchkbd/matchmouse + pipe reply)
  -> drawbridge.c _displayrdmouse/_displayrdkbd recv from Channel
  -> acme's mousethread/keyboardthread get events as before
```

### Thread model

- **Main thread**: SDL3 event loop (required by SDL3) -- `gfx_main()` ->
  `sdlloop()`
- **Acme proc**: spawned from `gfx_started()`, runs all acme logic
- Drawing calls from acme threads go through `draw_datawrite()` (protected
  by `drawlk` mutex)
- Flush dispatches to SDL3 thread via `SDL_PushEvent` (same as current
  devdraw)

### Build trick

The linker resolves symbols from `.o` files before `.a` libraries. So
`drawbridge.o` (with our `_displayconnect`, etc.) gets linked before
`libdraw.a`, overriding the pipe-based versions in `drawclient.o`. No
library modifications needed.

## Functions in drawbridge.c

These replace the pipe-based RPC implementations from `drawclient.c`:

```c
// Replaces _displayconnect: no fork/exec, just initialize the Client
int _displayconnect(Display *d);

// Replaces _displayinit: calls sdlattach directly (via cross-thread dispatch)
int _displayinit(Display *d, char *label, char *winsize);

// Replaces _displaywrdraw: calls draw_datawrite directly
int _displaywrdraw(Display *d, void *v, int n);

// Replaces _displayrddraw: calls draw_dataread directly
int _displayrddraw(Display *d, void *v, int n);

// Replaces _displayrdmouse: blocks on Channel, receiving from SDL3 event loop
int _displayrdmouse(Display *d, Mouse *m, int *resized);

// Replaces _displayrdkbd: blocks on Channel, receiving from SDL3 event loop
int _displayrdkbd(Display *d, Rune *r);

// Direct calls (no serialization needed):
int _displaymoveto(Display *d, Point p);
int _displaycursor(Display *d, Cursor *c, Cursor2 *c2);
int _displaybouncemouse(Display *d, Mouse *m);
int _displaylabel(Display *d, char *label);
char *_displayrdsnarf(Display *d);
int _displaywrsnarf(Display *d, char *snarf);
int _displaytop(Display *d);
int _displayresize(Display *d, Rectangle r);
```

## Changes in srv-direct.c (vs srv.c)

**Removed:**
- `serveproc`, `replymsg`, `runmsg`, `listenproc`
- `matchkbd`, `matchmouse` (pipe-based event matching)
- `threadmain` (acme has its own)
- All Wsysmsg serialization/deserialization

**Kept:**
- `gfx_keystroke`, `gfx_mousetrack`, `gfx_abortcompose`, `gfx_mouseresized`
- `client0` and Client initialization
- `kputc`

**Modified:**
- `gfx_keystroke`/`gfx_mousetrack` -- deliver events via `Channel*` send
  instead of `matchkbd`/`matchmouse` + pipe reply
- `gfx_started` -- signals readiness via `Rendez` instead of spawning
  `serveproc`

## Initialization Sequence

Current:
```
acme threadmain -> initdraw -> fork/exec devdraw
devdraw threadmain -> gfx_main -> SDL_Init -> sdlloop (blocks forever)
```

New:
```
threadmain (main.c):
  memimageinit()
  create Channel* for kbd/mouse
  proccreate(acme_main_proc, ...)   // acme logic in new proc
  gfx_main()                        // SDL event loop on main thread, blocks forever

acme_main_proc:
  wait for SDL3 readiness (Rendez)
  initdraw (-> drawbridge.c, no fork/exec)
  ... normal acme initialization ...
```

## Milestones

1. **Build system** -- mkfile compiles everything and links. Verify it links.
2. **drawbridge.c** -- direct `_displaywrdraw`/`_displayrddraw` calling
   `draw_datawrite`. Test with a simple draw program.
3. **Event delivery** -- channel-based keyboard/mouse in `srv-direct.c`.
   Test input.
4. **Full acme** -- restructured `threadmain`, everything integrated.
5. **Polish** -- resize, DPI, clipboard, edge cases.

## Risks and Mitigations

1. **Initialization ordering**: SDL3 must init before acme calls `initdraw`.
   Solved with a `Rendez` to synchronize.

2. **Thread safety**: Already handled -- `drawlk` protects memdraw, SDL3
   dispatch uses `SDL_PushEvent`. Same model as current devdraw.

3. **`Display.buf` concurrency**: Preserved via existing `_pin`/`_unpin`
   pattern from acme's libthread usage.

4. **SDL3 main thread requirement**: SDL3 requires `SDL_Init` and rendering
   on the same thread. The current design already handles this with
   `SDL_PushEvent` for cross-thread dispatch. We preserve this exactly.

## Critical Files for Implementation

- `src/libdraw/drawclient.c` -- the file being replaced; every `_display*`
  function signature must be matched exactly
- `src/cmd/devdraw/srv.c` -- event delivery logic that must be refactored
  for channel-based delivery
- `src/cmd/devdraw/devdraw.c` -- `draw_datawrite`/`draw_dataread` and all
  state management called directly
- `src/cmd/devdraw/sdl3-screen.c` -- SDL3 backend with `gfx_main`, event
  loop, `rpc_attach`, and flush dispatch
- `src/libdraw/init.c` -- `_initdisplay`/`geninitdraw`/`flushimage`/
  `bufimage` flow that must work with the direct backend

---

# Phase 2: Embedded Fontsrv

## Goal

Eliminate the `fontsrv` fork/exec dependency. System TrueType/OpenType fonts
(accessed via `/mnt/font/` paths) are rendered in-process using FreeType and
fontconfig, linked directly into the acme binary.

## Design

Replace `_fontpipe()` (which fork/execs `fontsrv -pp`) with an in-process
version that creates a pipe, spawns a writer thread, and generates font data
using fontsrv's FreeType rendering engine linked directly.

### New files

| File | Purpose |
|------|---------|
| `fontpipe.c` | Embedded `_fontpipe()`: parses font path, spawns writer proc, generates font/subfont data via pipe |
| `openfont.c` | Local copy of `libdraw/openfont.c` without `_fontpipe` (prevents linker conflict) |
| `getsubfont.c` | Local copy of `libdraw/getsubfont.c` (same reason) |

### Reused from fontsrv (compiled from `../fontsrv/`)

- `x11.c` -- FreeType/fontconfig rendering: `loadfonts()`, `load()`, `mksubfont()`
- `pjw.c` -- Peter J. Weinberger glyph for missing characters

### Font data generation

For font header files (`/mnt/font/Name/Size/font`):
- Generate height, ascent, and subfont file listing as text

For subfont bitmap files (`/mnt/font/Name/Size/x??????.bit`):
- Call `mksubfont()` to render glyphs with FreeType
- Write uncompressed Plan 9 image header + pixel data + packed Fontchar info

### Symbol conflicts resolved

- `emalloc9p` -- provided in `fontpipe.c` (normally from lib9p server library)
- `xfont`, `nxfont` -- defined in `fontpipe.c` (normally in fontsrv's main.c)
- `fontcmp` -- defined in `fontpipe.c` for qsort

### Thread safety

- FreeType operations (`load()`, `mksubfont()`) serialized with `fontsrv_lk` QLock
- Initialization (`loadfonts()` + sort) happens once on first `_fontpipe()` call
- Writer proc uses `proccreate()` for pipe-based data delivery

---

# Phase 3: Embedded Plumber

## Goal

Eliminate the external `plumber` daemon dependency. Plumbing rules are loaded
and matched in-process, so middle-click on filenames, URLs, etc. works without
a separate plumber process.

## Design

The plumber's rule parsing engine (`rules.c`) and matching engine (`match.c`)
are adapted with renamed symbols to avoid conflicts with acme's own symbols,
then linked directly. The `look3()` function calls the rule matcher inline
instead of sending messages to an external plumber via 9P.

### New files

| File | Purpose |
|------|---------|
| `plumb-internal.h` | Adapted `plumber.h` with renamed symbols |
| `plumb-rules.c` | Adapted `rules.c` -- rule parsing, variable expansion |
| `plumb-match.c` | Adapted `match.c` -- pattern matching, rewriting, exec |
| `plumb-glue.c` | Globals, `plumb_init()`, `plumb_send()`, utility functions |
| `look.c` | Modified `acme/look.c` with embedded plumbing |

### Symbol conflicts resolved

| Plumber symbol | Acme symbol | Resolution |
|---------------|-------------|------------|
| `expand(Exec*, char*, char**)` | `expand(Text*, uint, uint, Expand*, int)` | Renamed to `plumb_expand` |
| `emalloc(long)` | `emalloc(uint)` | Renamed to `plumb_emalloc` |
| `erealloc(void*, long)` | `erealloc(void*, uint)` | Renamed to `plumb_erealloc` |
| `estrdup(char*)` | `estrdup(char*)` | Renamed to `plumb_estrdup` |
| `error(char*, ...)` | `error(char*)` | Renamed to `plumb_error` |
| `lookup(char*, char*[])` | `lookup(...)` in exec.c | Made `static` |

### Plumbing flow (embedded)

```
look3() builds Plumbmsg
  -> plumb_send(msg) tries each ruleset
     -> plumb_matchruleset(msg, ruleset)
        -> matchpat() for each pattern
        -> rewrite() applies transformations
     -> if port == "edit": return 0 (caller calls plumblook)
     -> if has start action: plumb_startup() forks program, return 1
     -> no match: return -1
  -> fallthrough: built-in file opening
```

### Initialization

`startplumbing()` calls `plumb_init()` which:
1. Reads `~/lib/plumbing` (or `$PLAN9/plumb/initial.plumbing`)
2. Parses rules via `plumb_readrules()`
3. Sets `plumbing_ready` flag

### What changed in look.c

- `plumbthread()` removed (no external plumber connection needed)
- `startplumbing()` calls `plumb_init()` instead of connecting to plumber daemon
- `look3()` calls `plumb_send()` instead of `plumbsendtofid()`
- "to edit" delivery: `plumblook()` called directly (no channel relay)
- "start" actions: `plumb_startup()` fork/execs programs as before

---

## Runtime Dependencies

The standalone acme binary requires only shared libraries:
- `libSDL3` -- graphics, input, clipboard
- `libfontconfig` + `libfreetype` -- system font discovery and rendering
- `libz` -- compression (freetype dependency)
- Standard: `libc`, `libm`, `libresolv`

No external programs needed: no `devdraw`, no `fontsrv`, no `plumber`.

## Future Work

Eliminate the inner draw protocol. Each libdraw function would call memdraw
directly instead of serializing opcodes into `Display.buf`:
- `allocimage` -> `allocmemimage` + image table management
- `draw` -> `memdraw` + flush tracking
- `flushimage` -> direct texture upload dispatch

This is a much larger refactor touching every file in libdraw.
