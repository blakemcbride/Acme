# Acme Keyboard Reference

## Cursor Movement

| Key            | Action                                      |
|----------------|---------------------------------------------|
| Left Arrow     | Move cursor one character left               |
| Right Arrow    | Move cursor one character right              |
| Up Arrow       | Move cursor up one line (no scroll)          |
| Down Arrow     | Move cursor down one line (no scroll)        |
| Ctrl+Left      | Move cursor to beginning of line             |
| Ctrl+Right     | Move cursor to end of line                   |

The up and down arrow keys maintain a sticky column position.
When moving through short lines, the cursor temporarily adjusts
to the shorter line length but returns to the original column
when a longer line is reached.  The sticky column resets on any
key other than up/down or on a mouse click.

If the cursor is on the last visible line and down is pressed,
the text scrolls down one line and the cursor moves to the new
last visible line.  Similarly, pressing up on the first visible
line scrolls up one line.

## Scrolling

| Key            | Action                                      |
|----------------|---------------------------------------------|
| Page Up        | Scroll up one full screen                    |
| Page Down      | Scroll down one full screen                  |
| Ctrl+Up        | Scroll up one full screen                    |
| Ctrl+Down      | Scroll down one full screen                  |

Page Down does nothing if the end of the file is already visible.

## Jump to Position

| Key            | Action                                      |
|----------------|---------------------------------------------|
| Home           | Cursor to first character visible on screen  |
| End            | Cursor to beginning of last visible line     |
| Ctrl+Home      | Jump to beginning of file                    |
| Ctrl+End       | Jump to end of file                          |
| Ctrl+Page Up   | Jump to beginning of file                    |
| Ctrl+Page Down | Jump to end of file                          |

Home and End do not scroll the text.  Ctrl+Home, Ctrl+End,
Ctrl+Page Up, and Ctrl+Page Down scroll as needed to show the
target position and place the cursor there.

## Editing

| Key            | Action                                      |
|----------------|---------------------------------------------|
| Backspace      | Delete character to the left of cursor       |
| Delete         | Delete character to the right of cursor      |
| Ctrl+H         | Delete character (same as Backspace)         |
| Ctrl+W         | Delete word                                  |
| Ctrl+U         | Delete to beginning of line                  |
| Ctrl+A         | Move to beginning of line                    |
| Ctrl+E         | Move to end of line                          |
