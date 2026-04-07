# Building Acme on Linux

## Prerequisites

### Fedora

```
sudo dnf install gcc make SDL3-devel freetype-devel fontconfig-devel zlib-devel
```

### Ubuntu / Debian

```
sudo apt install gcc make libsdl3-dev libfreetype-dev libfontconfig-dev zlib1g-dev
```

### Arch Linux

```
sudo pacman -S gcc make sdl3 freetype2 fontconfig zlib
```

`pkg-config` is used to locate SDL3, freetype, and fontconfig headers
and libraries. It is typically installed by default; if not, install it
with your package manager.

## Building

```
make
```

The binary is placed in `bin/acme`.

### Clean

```
make clean      # remove .o files and bin/acme
make realclean  # also remove lib/*.a files
```

## Running

```
./bin/acme
```

No environment variables or configuration files are required. Acme
will open a window and is ready to use.

Fonts are discovered via fontconfig/freetype. The default plumbing
rules use `xdg-open` for URLs, images, and PDFs.

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
Scroll up/down maps to buttons 4 and 5.

## Notes

- The SDL3 backend supports both X11 and Wayland natively. No X11
  or XWayland dependency is required on Wayland systems.
- Clipboard uses SDL3's clipboard API (`Snarf`/`Paste` commands
  within acme, Ctrl+C/Ctrl+V in other apps). PRIMARY selection
  (middle-click paste) is not available; only CLIPBOARD is used.
- The build produces a single self-contained binary with no runtime
  dependencies on Plan 9 tools or the `$PLAN9` environment variable.
- The Win command spawns a shell via PTY using `$SHELL` (typically
  `/bin/bash`).
