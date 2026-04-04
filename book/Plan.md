# Plan: Comprehensive Acme Editor User Manual

## Book Specifications

- **Format:** LaTeX (memoir or book class)
- **Trim size:** 6" x 9" (US Trade)
- **Output:** PDF
- **Audience:** Programmers new to acme, with some assumed Unix literacy
- **Tone:** Practical and direct, matching acme's own philosophy

## Sources

### Internal (this repository)

- `README.md` -- project overview, build instructions
- `CLAUDE.md` -- architecture, build system, SDL3 backend design
- `StandAlone.md` -- embedded devdraw/fontsrv/plumber architecture
- `docs/KeyBoard.md` -- keyboard navigation reference
- `docs/CheatSheet.txt` -- command and mouse quick reference
- `macOS.md` -- macOS build and mouse notes
- `Windows.md` -- Windows/MSYS2 build instructions
- `lib/keyboard` -- Unicode composition table (588 entries)
- `src/cmd/acme/exec.c` -- all built-in commands and their flags
- `src/cmd/acme/acme.c` -- command-line flags and startup
- `src/cmd/acme/edit.c`, `ecmd.c` -- Edit (sam) command implementation
- `src/cmd/acme/text.c` -- mouse chords, text selection, Button 3 logic
- `src/cmd/acme/scrl.c` -- scrollbar behavior
- `src/cmd/acme-sdl3/win.c` -- Win command implementation
- `src/cmd/acme-sdl3/plumb-glue.c` -- default plumbing rules
- `src/cmd/acme-sdl3/pipecmd.c` -- pipe command (< > |) implementation
- `src/cmd/devdraw/sdl3-screen.c` -- key mapping, modifier-click emulation
- `LICENSE` -- MIT (Plan 9 Foundation, Russ Cox, Google)

### External

- Rob Pike, "Acme: A User Interface for Programmers," Proc. Winter
  1994 USENIX Conference.
  https://research.swtch.com/acme.pdf
- Rob Pike, "The Text Editor sam," Software -- Practice and Experience,
  Vol 17, No 11, November 1987.
  https://doc.cat-v.org/plan_9/4th_edition/papers/sam/
- Rob Pike, "Structural Regular Expressions," Proc. EUUG Spring 1987
  Conference, Helsinki.
  https://doc.cat-v.org/bell_labs/structural_regexps/
- Russ Cox, "A Tour of Acme" (screencast).
  https://research.swtch.com/acme
  https://www.youtube.com/watch?v=dP1xVpMPn8M
- Plan 9 acme(1) man page.
  https://9fans.github.io/plan9port/man/man1/acme.html
- Plan 9 sam(1) man page.
  https://9fans.github.io/plan9port/man/man1/sam.html
- Rob Pike, "Plumbing and Other Utilities" (plumber paper).
  https://doc.cat-v.org/plan_9/4th_edition/papers/plumb/
- Rob Pike, "A Minimalist Global User Interface" (rio paper).
  https://doc.cat-v.org/plan_9/4th_edition/papers/rio/
- Plan 9 Fourth Edition collected papers.
  https://doc.cat-v.org/plan_9/4th_edition/papers/
- plan9port project.
  https://github.com/9fans/plan9port
- Community acme resources.
  https://acme.cat-v.org/

---

## Chapter Outline

### Front Matter

- Title page
- Copyright / license notice (MIT, Plan 9 Foundation)
- Table of contents
- List of figures
- List of tables

### Chapter 1: Introduction

What acme is and why it exists. Its lineage from Plan 9 and Rob
Pike's design philosophy: text as interface, mouse as primary input,
composability over features. How acme differs from vi/emacs/VS Code.
The "anti-IDE" concept -- acme provides a surface, not a framework.

Brief history: Bell Labs, Plan 9, plan9port, this standalone fork.

**Sources:** Pike's 1994 acme paper (design rationale), rio paper
(UI philosophy), README.md (project goals).

### Chapter 2: Installation

#### 2.1 Linux
Prerequisites (GCC, GNU make, SDL3, FreeType, fontconfig, zlib).
Package manager commands for Debian/Ubuntu, Fedora, Arch. Building
from source. Verifying the build.

#### 2.2 macOS
Homebrew prerequisites. Xcode Command Line Tools. Building. Notes
on font discovery and Homebrew paths.

#### 2.3 Windows
MSYS2 installation. Why MSYS2 MSYS shell (not MinGW). Package
installation. Building and running. Runtime dependencies
(msys-2.0.dll).

#### 2.4 Command-Line Options
All flags: `-a`, `-b`, `-c`, `-f`, `-F`, `-l`, `-W`, `-r`.
Environment variables: `$acmeshell`, `$tabstop`, `$NAMESPACE`,
`$HOME`.

**Sources:** README.md, macOS.md, Windows.md, acme.c (flag parsing).

### Chapter 3: The Acme Screen

Anatomy of the display: the tag bar, the body, the scrollbar, the
layout button, the dirty indicator. Columns and windows. How text
flows. The scratch area concept.

Illustrated with annotated screenshots showing each element labeled.

**Sources:** rows.c, cols.c, wind.c, dat.h (color indices).

### Chapter 4: The Mouse

The most important chapter. Acme is a mouse-driven editor.

#### 4.1 The Three Buttons
Button 1 (select), Button 2 (execute), Button 3 (look/open).
What each does when clicked in the tag, in the body, in the
scrollbar.

#### 4.2 Selection
Click, drag, double-click (word), triple-click (line). How
selection expansion works for filenames, addresses, quoted strings.

#### 4.3 Executing Commands (Button 2)
Clicking on built-in commands. Clicking on arbitrary text to run
shell commands. How the command text is identified (expand to
whitespace boundaries).

#### 4.4 Opening and Searching (Button 3)
Filenames, file:line addresses, :address syntax, <include.h>,
literal text search. How repeated clicks continue a search.

#### 4.5 Mouse Chords
Cut (1-2), Paste (1-3), Snarf (1-2-3). The 2-1 argument chord.
Detailed explanation with diagrams showing finger positions.

#### 4.6 The Scrollbar
Button 1 (up), Button 2 (drag/jump), Button 3 (down). The `-r`
flag to reverse. The thumb indicator.

#### 4.7 Scroll Wheel
Scroll up/down behavior.

#### 4.8 One-Button and Two-Button Mice
Modifier-click emulation: Ctrl-click = Button 2, Alt-click =
Button 3. Relevant for Mac trackpads and laptops.

**Sources:** text.c (selection, B3 logic), exec.c (B2 execution),
scrl.c (scrollbar), sdl3-screen.c (modifier emulation, button
mapping), CheatSheet.txt, Pike's acme paper (Sections 3-4).

### Chapter 5: Keyboard

#### 5.1 Cursor Movement
Arrow keys, Ctrl+arrow, Home, End, Ctrl+Home, Ctrl+End.
Sticky column behavior for up/down.

#### 5.2 Page Navigation
Page Up, Page Down, Ctrl+Page Up, Ctrl+Page Down.

#### 5.3 Editing Keys
Backspace, Delete, Ctrl+H, Ctrl+W, Ctrl+U, Ctrl+A, Ctrl+E.

#### 5.4 Text Entry
Normal typing. Auto-indent behavior. Tab handling.

#### 5.5 Unicode Composition
The Alt key initiates composition. Multi-key sequences for accented
characters, Greek, Cyrillic, math symbols, arrows, etc. Reference
table of common compositions.

**Sources:** KeyBoard.md, sdl3-screen.c (key mapping), lib/keyboard
(composition table), latin1.c.

### Chapter 6: Built-in Commands

Comprehensive reference for every built-in command, grouped by
function.

#### 6.1 Clipboard: Cut, Snarf, Paste
The snarf buffer. System clipboard integration via SDL3.

#### 6.2 Undo and Redo
Sequence-based undo. How undo interacts with multiple views of the
same file.

#### 6.3 File Operations: Get, Put, Putall
Reloading, saving, saving all. Dirty checking. The Put argument
for "save as."

#### 6.4 Window Management: New, Del, Delete, Zerox, Newcol, Delcol, Sort
Creating, closing, cloning, organizing windows.

#### 6.5 Session Management: Dump, Load, Exit
Saving and restoring the full acme state.

#### 6.6 Search: Look
Literal text search. Using Look with arguments.

#### 6.7 Display: Font, Tab, Indent
Toggling fonts. Setting tab stops. Auto-indentation control.

#### 6.8 Process Control: Kill, ID
Managing running commands.

#### 6.9 Include Paths: Incl
Adding directories for <file.h> resolution.

**Sources:** exec.c (command table, flag bits, implementations),
CheatSheet.txt, acme(1) man page.

### Chapter 7: Shell Commands and Pipes

#### 7.1 Running External Commands
Any non-built-in text executed with Button 2 runs as a shell
command. Working directory. Environment variables set: `$winid`,
`$%`, `$samfile`.

#### 7.2 Pipe Prefixes: <, >, |
Replace selection with output. Send selection as input. Filter
selection through command. Practical examples: `|sort`, `|fmt -w 72`,
`<date`, `>wc`.

#### 7.3 The +Errors Window
Where stderr and error output appears. Per-directory error windows.

#### 7.4 The Win Command
Interactive shell in an acme window. How it works (PTY, bash).
Typing and sending input. The Send command. Differences from a
real terminal.

**Sources:** exec.c (command execution), pipecmd.c (pipe
implementation), win.c (Win/PTY), CheatSheet.txt.

### Chapter 8: The Edit Command

The sam editing language embedded in acme. This is the power-user
chapter.

#### 8.1 Addresses
Dot, line numbers (#n), character offsets, regular expressions
(/re/), end of file ($). Compound addresses with comma and
semicolon. Relative addresses with + and -.

#### 8.2 Simple Commands
`a` (append), `c` (change), `d` (delete), `i` (insert),
`s/re/text/` (substitute).

#### 8.3 Structural Commands
`x/re/command` (extract), `y/re/command` (complement of x),
`g/re/command` (guard), `v/re/command` (inverse guard).

#### 8.4 Move and Copy
`m` (move to address), `t` (copy to address).

#### 8.5 Multi-file Commands
`X/re/command` (for each file matching), `Y/re/command` (inverse).

#### 8.6 Grouping
Braces `{ }` to group multiple commands.

#### 8.7 Worked Examples
- Delete all blank lines: `Edit ,x/\n\n+/c/\n/`
- Indent selected code: `Edit ,x/.*\n/i/\t/`
- Rename a variable: `Edit ,s/oldname/newname/g`
- Delete trailing whitespace: `Edit ,s/[ \t]+$//g`
- Extract function signatures: `Edit ,x/^[a-z].*\n{/p`

**Sources:** edit.c, ecmd.c, regx.c, sam(1) man page, Pike's sam
paper, "Structural Regular Expressions" paper.

### Chapter 9: Plumbing

#### 9.1 What Plumbing Does
Inter-application message passing. How Button 3 uses plumbing to
decide what to do with clicked text.

#### 9.2 Default Rules
URLs open in browser. Images and PDFs open in viewer. File
addresses open in acme. Include files searched in system paths.

#### 9.3 Custom Rules
Writing `~/lib/plumbing`. Rule syntax: type, data, arg, plumb
directives. Pattern matching. The `plumb start` and `plumb client`
actions.

#### 9.4 Platform Differences
`xdg-open` (Linux), `open` (macOS), `cygstart` (Windows/MSYS2).

**Sources:** plumb-glue.c (default rules), plumb-rules.c, plumb-
match.c, plumber man page, Pike's plumber paper.

### Chapter 10: Practical Workflows

#### 10.1 Editing Code
Opening a project. Navigating with Button 3. Searching. Using
multiple columns. Compiler error workflow (click on file:line in
+Errors).

#### 10.2 Building and Testing
Running make from a Win window or via Button 2. Error navigation.
Iterative edit-compile cycles.

#### 10.3 Version Control
Running git commands. Viewing diffs. Committing.

#### 10.4 Writing Prose
Using proportional fonts. Word wrapping with `|fmt`. Spell
checking.

#### 10.5 Multi-File Search and Replace
Using Edit X command. Grep + plumbing workflow.

#### 10.6 Session Management
Using Dump/Load to preserve workspace across sessions.

**Sources:** Pike's acme paper (Section 5, examples), Russ Cox's
screencast, CheatSheet.txt.

### Chapter 11: Customization

#### 11.1 Fonts
The `-f` and `-F` flags. The Font command. Fontconfig font names.
Available font sizes.

#### 11.2 Plumbing Rules
Custom `~/lib/plumbing` file. Adding language-specific rules
(e.g., Go import paths, Python modules, Rust crate docs).

#### 11.3 Helper Scripts
Writing shell scripts that integrate with acme via `$winid` and
the environment. Examples: auto-formatting, linting, test runners.

#### 11.4 The acmeshell Variable
Choosing a different shell for command execution.

**Sources:** acme.c (font/flag handling), plumb-glue.c, exec.c
(environment setup).

### Appendices

#### Appendix A: Command Reference
Alphabetical table of all built-in commands with synopsis, arguments,
and behavior flags (mark dirty, use selection, apply to body).
One page per command or dense table format.

**Source:** exec.c command table.

#### Appendix B: Edit Command Reference
Complete sam command language syntax in tabular form. Address types,
commands, flags.

**Source:** sam(1) man page, edit.c, ecmd.c.

#### Appendix C: Mouse Reference
Table: every mouse action (button, location, modifier) and its
effect. Chord diagrams.

**Source:** text.c, scrl.c, CheatSheet.txt.

#### Appendix D: Keyboard Reference
Table: every keyboard shortcut and its action.

**Source:** KeyBoard.md, sdl3-screen.c.

#### Appendix E: Unicode Composition Table
Full table of Alt-key composition sequences from `lib/keyboard`,
organized by category (Latin accents, Greek, Cyrillic, math,
arrows, etc.).

**Source:** lib/keyboard.

#### Appendix F: Default Plumbing Rules
The complete built-in rule set with annotations explaining each
rule.

**Source:** plumb-glue.c defaultrules[].

### Back Matter

- Bibliography (Pike papers, man pages, Cox screencast, plan9port)
- Index

---

## LaTeX Structure

```
book/
  main.tex          -- document class, geometry, preamble, \includes
  ch01-intro.tex
  ch02-install.tex
  ch03-screen.tex
  ch04-mouse.tex
  ch05-keyboard.tex
  ch06-commands.tex
  ch07-shell.tex
  ch08-edit.tex
  ch09-plumbing.tex
  ch10-workflows.tex
  ch11-customize.tex
  appA-cmdref.tex
  appB-editref.tex
  appC-mouseref.tex
  appD-kbdref.tex
  appE-unicode.tex
  appF-plumbing.tex
  biblio.tex
  figures/           -- screenshots and diagrams
  Makefile           -- pdflatex build
```

### LaTeX Preamble Notes

```latex
\documentclass[11pt,openany]{memoir}
\usepackage[paperwidth=6in,paperheight=9in,
            inner=0.75in,outer=0.625in,
            top=0.75in,bottom=0.75in]{geometry}
\usepackage{fontspec}          % XeLaTeX for Unicode
\usepackage{listings}          % code listings
\usepackage{booktabs}          % tables
\usepackage{graphicx}          % figures
\usepackage{hyperref}          % cross-references
\usepackage{makeidx}           % index
\setmainfont{TeX Gyre Pagella} % or similar serif
\setmonofont{DejaVu Sans Mono} % match acme's default
```

Build with `xelatex` (for Unicode/fontspec support).

---

## Estimated Scope

| Section | Pages |
|---------|-------|
| Front matter | 6 |
| Ch 1: Introduction | 8 |
| Ch 2: Installation | 10 |
| Ch 3: The Acme Screen | 8 |
| Ch 4: The Mouse | 20 |
| Ch 5: Keyboard | 10 |
| Ch 6: Built-in Commands | 16 |
| Ch 7: Shell Commands and Pipes | 12 |
| Ch 8: The Edit Command | 18 |
| Ch 9: Plumbing | 10 |
| Ch 10: Practical Workflows | 14 |
| Ch 11: Customization | 8 |
| Appendix A: Command Reference | 6 |
| Appendix B: Edit Reference | 4 |
| Appendix C: Mouse Reference | 4 |
| Appendix D: Keyboard Reference | 3 |
| Appendix E: Unicode Table | 12 |
| Appendix F: Plumbing Rules | 3 |
| Bibliography + Index | 6 |
| **Total** | **~178** |

---

## Production Order

1. Set up LaTeX skeleton (main.tex, geometry, preamble)
2. Write Chapter 4 (Mouse) first -- it is the heart of acme
3. Write Chapter 8 (Edit command) -- the deepest material
4. Write Chapters 5-7 (Keyboard, Commands, Shell)
5. Write Chapter 3 (Screen anatomy, with screenshots)
6. Write Chapter 9 (Plumbing)
7. Write Chapter 10 (Workflows)
8. Write Chapters 1-2 (Introduction, Installation)
9. Write Chapter 11 (Customization)
10. Compile appendices from source data
11. Build bibliography and index
12. Review, copyedit, final proof
