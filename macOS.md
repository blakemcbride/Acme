# Building Acme on macOS

## Prerequisites

Install the required dependencies via Homebrew:

```
brew install sdl3 fontconfig freetype pkg-config
```

Xcode Command Line Tools must be installed (provides clang, make, etc.):

```
xcode-select --install
```

## Building

```
make
```

The binary is placed in `bin/acme`.

## Running

```
./bin/acme
```

Fonts are discovered via fontconfig/freetype, same as on Linux.
The default plumbing rules use macOS `open` command (instead of
Linux `xdg-open`) for URLs, images, and PDFs.

Custom plumbing rules can be placed in `~/lib/plumbing` to override
the built-in defaults.

## Mouse

Acme requires a three-button mouse. Most Mac users have a one-button
trackpad or Magic Mouse. The following options are available:

**Modifier-click emulation (built in):**
- Ctrl-click = middle button (button 2) -- execute
- Alt/Option-click = right button (button 3) -- acquire/look

**USB mouse:**
Any standard USB or Bluetooth three-button mouse works natively.
This is strongly recommended for extended use. Acme's mouse-driven
interface is far more comfortable with real buttons.

**Magic Mouse / trackpad:**
These only provide left-click and right-click (two buttons).
Use Ctrl-click for the middle button. Right-click maps to button 3.

**Scroll wheel:**
Scroll up/down maps to buttons 4 and 5 (same as Linux).

## Notes

- The build uses `pkg-config` to locate SDL3, freetype, and fontconfig
  headers and libraries from Homebrew. If `pkg-config` is not available,
  it falls back to default Homebrew paths (`/opt/homebrew` on Apple
  Silicon, `/usr/local` on Intel Macs).
- Clipboard uses SDL3's clipboard API (Cmd+C/Cmd+V in other apps,
  `Snarf`/`Paste` commands within acme).
- The SDL3 backend provides native macOS window management without
  requiring X11 or XQuartz.
