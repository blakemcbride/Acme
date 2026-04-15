# Standalone Acme Editor for Linux and Windows

A self-contained build of the [acme](http://acme.cat-v.org) text editor
that runs on modern Linux (X11 or Wayland) and Windows systems with no
external dependencies on Plan 9 tools or infrastructure.

## What is Acme?

Acme is a programmer's text editor and development environment created
by Rob Pike at Bell Labs as part of the Plan 9 operating system.  It
uses a mouse-driven interface where any text can be executed as a
command or used as an address.  It remains one of the most distinctive
and productive editing environments ever built.

## Origin

This project is derived from
[plan9port](https://github.com/9fans/plan9port) (Plan 9 from User
Space), Russ Cox's port of Plan 9 tools to Unix/Linux.  The original
plan9port is a large collection of programs and libraries that depend
on the Plan 9 build system (`mk`, `9c`, `9l`), the `PLAN9` environment
variable, and X11 for graphics.

## What Changed

All of the modifications in this repository were done by
[Claude Code](https://claude.ai/claude-code) (Anthropic's AI coding
agent), with direction and guidance from
[Blake McBride](https://github.com/blakemcbride).

The changes, in rough order:

1. **X11 to SDL3** — Replaced the X11 graphics backend in `devdraw`
   with SDL3, providing native Wayland support and eliminating the X11
   dependency.  The new backend (`sdl3-screen.c`, `sdl3-draw.c`) uses
   software rendering via memdraw with texture upload at flush, matching
   the design of the macOS backend.  Since SDL3 is portable to macOS
   and Windows, this change opens the door to future cross-platform
   builds.

2. **Embedded devdraw** — The `devdraw` graphics server, previously a
   separate process, is compiled directly into the acme binary.

3. **Embedded fontsrv** — Font serving via FreeType and fontconfig is
   built into the binary.  No external `fontsrv` process needed.

4. **Embedded plumber** — Plumbing rules (the Plan 9 mechanism for
   dispatching actions on text) are compiled into the binary with
   sensible defaults for Linux (using `xdg-open` for URLs, images, and
   PDFs).  Users can still override rules via `~/lib/plumbing`.

5. **GNU make build system** — Replaced the Plan 9 `mk` build system
   entirely.  All 18 static libraries and the acme binary are built
   from source using `gcc` and GNU `make`.  No Plan 9 build tools
   (`mk`, `9c`, `9l`, `9ar`) are needed.

6. **No PLAN9 environment variable** — The runtime dependency on
   `$PLAN9` has been eliminated.

7. **Removed everything acme doesn't need** — The hundreds of other
   Plan 9 commands and tools have been stripped out.  What remains is
   acme and the libraries it depends on.

8. **Built-in Win command** — Type `Win` in a tag bar and middle-click
   it to open an interactive shell session inside an acme window.
   Commands are typed directly into the buffer and executed by pressing
   Enter.  Output appears inline.  This replaces the original Plan 9
   `win` program, which required 9P infrastructure.

9. **Pipe commands without 9P** — Pipe commands (`|sort`, `<date`,
   `>wc`) work using direct pipes and temp files instead of the
   original 9P filesystem.  Select text, type `|sort`, and
   middle-click it to sort the selection in place.

10. **Comprehensive user manual** — A full-length book in the `book/`
    directory, written in LaTeX for 6"x9" print format.  Covers all
    aspects of acme from basic mouse operations to the sam editing
    language, plumbing, and customization.

11. **Conventional keyboard navigation** — Arrow keys, Home, End, Page
    Up, Page Down, and Delete work as expected in a modern editor.  Up
    and down arrows move the cursor one line at a time with sticky
    column tracking.  Ctrl+arrow combinations provide additional
    navigation (beginning/end of line, page scroll, jump to start/end
    of file).  See `KeyBoard.md` for the full reference.

12. **Tilde expansion in file paths** — File paths beginning with `~/`
    are expanded to the user's home directory.  Type `~/src/foo.c` in
    a tag and `Get` or right-click it to open the file.

## Building

The codebase supports three build targets, all from the same source
tree (platform differences are guarded by `#ifdef`):

### Linux (native)

See `Linux.md` for detailed instructions.

```
sudo dnf install gcc make SDL3-devel freetype-devel fontconfig-devel zlib-devel  # Fedora
sudo apt install gcc make libsdl3-dev libfreetype-dev libfontconfig-dev zlib1g-dev  # Debian/Ubuntu
make
```

Dynamically linked against system libraries.

### Windows / MSYS2 (POSIX runtime)

See `Windows-MSYS.md` for detailed instructions.  Requires MSYS2 at
both build and runtime.  Uses the MSYS POSIX layer for fork, exec,
pipes, and PTY.

```
make
```

### Windows (native, standalone)

See `Windows-native.md`.  Produces a single standalone `acme.exe`
with no MSYS2 or Cygwin runtime dependency — only Windows system
DLLs.  Built with mingw64 gcc and statically linked.

```
make native-win
```

The resulting `bin/acme.exe` can be copied to any Windows 10/11 machine
and run with no other files required.

A pre-compiled Windows binary is available at
[https://blakemcbride.com/files/acme.exe](https://blakemcbride.com/files/acme.exe).
Download it and run directly — no installation or dependencies needed.

### Clean

```
make clean      # remove .o files and bin/acme
make realclean  # also remove lib/*.a files
```

## Running

```
bin/acme
```

No environment variables or configuration files are required.  Acme
will open a window and is ready to use.

## Learning Acme

Acme's interface is unusual.  Here are the essentials:

- **Three mouse buttons** — left selects, middle executes, right searches/opens
- **Any text is a command** — type `ls -l` anywhere, middle-click it to run
- **Pipe commands** — select text, type `|sort`, middle-click to sort in place
- **Tag bar** — the top line of each window contains commands you can execute
- **Right-click a filename** — opens it
- **Right-click `file:123`** — opens file at line 123
- **`Win` command** — middle-click `Win` in a tag to open an interactive shell

The `docs/` directory contains detailed documentation including Rob
Pike's original paper, the Plan 9 manual pages, keyboard and mouse
shortcut references, and a video tour.

The `book/` directory contains a comprehensive user manual in LaTeX
(6"x9" trim size).  Build it with `make` in that directory (requires
XeLaTeX and makeindex).  It covers installation, the mouse-driven
interface, keyboard navigation, built-in commands, shell integration,
the Edit command (sam language), plumbing, practical workflows, and
customization.

For a full introduction, see Russ Cox's
[tour of Acme](http://acme.cat-v.org/).

## License

This project inherits the plan9port license.  See the `LICENSE` file
for details.

