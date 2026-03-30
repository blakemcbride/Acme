# GNU Makefile for standalone acme (SDL3)
#
# Prerequisites: gcc, SDL3, FreeType, fontconfig development libraries.
#
# Usage:
#   make                Build acme
#   make install        Install to ../../bin/
#   make clean          Remove build artifacts

# PLAN9 root (two levels up from src/cmd/acme-sdl3)
PLAN9 ?= $(realpath ../../..)

CC = gcc
CFLAGS = -O2 -c \
	-I$(PLAN9)/include \
	-I/usr/include/freetype2 \
	-Wall \
	-Wno-array-parameter \
	-Wno-parentheses \
	-Wno-missing-braces \
	-Wno-switch \
	-Wno-comment \
	-Wno-sign-compare \
	-Wno-unknown-pragmas \
	-Wno-misleading-indentation \
	-Wno-stringop-truncation \
	-Wno-stringop-overflow \
	-Wno-format-truncation \
	-Wno-unused-but-set-variable \
	-Wno-deprecated-declarations \
	-fno-omit-frame-pointer \
	-fsigned-char \
	-fno-common \
	-std=gnu11 \
	-ggdb

LDFLAGS = -L$(PLAN9)/lib \
	-lSDL3 -lfontconfig -lfreetype -lz \
	-lsec -lauth -lauthsrv -lmemdraw -lndb -lplumb \
	-lmemlayer -lframe -ldraw -l9pclient -lip -lmp -lmux \
	-lthread -lregexp9 -lcomplete -lbio -l9 -lthread -l9 -lsec -l9 \
	-lm -lutil -lresolv -lpthread

TARGET = $(PLAN9)/bin/acme

# Source directories
ACME = ../acme
DEVDRAW = ../devdraw
FONTSRV = ../fontsrv
LIBDRAW = ../../libdraw

# Local sources (in this directory)
LOCAL_SRC = drawbridge.c srv-direct.c look.c fsys.c \
	fontpipe.c openfont.c getsubfont.c \
	plumb-glue.c plumb-rules.c plumb-match.c

# Acme sources (from ../acme/)
ACME_SRC = acme.c addr.c buff.c cols.c dat.c disk.c ecmd.c edit.c \
	elog.c exec.c file.c logf.c regx.c rows.c scrl.c text.c \
	time.c util.c wind.c xfid.c

# Devdraw sources (from ../devdraw/)
DEVDRAW_SRC = devdraw.c sdl3-draw.c sdl3-screen.c latin1.c mouseswap.c winsize.c

# All object files
LOCAL_OBJ = $(LOCAL_SRC:.c=.o)
ACME_OBJ = $(ACME_SRC:.c=.o)
DEVDRAW_OBJ = $(DEVDRAW_SRC:.c=.o)
FONTSRV_OBJ = fontsrv-x11.o pjw.o

OBJS = $(LOCAL_OBJ) $(ACME_OBJ) $(DEVDRAW_OBJ) $(FONTSRV_OBJ)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Local sources with special include paths
look.o: look.c
	$(CC) $(CFLAGS) -I$(ACME) look.c

fsys.o: fsys.c
	$(CC) $(CFLAGS) -I$(ACME) fsys.c

fontpipe.o: fontpipe.c
	$(CC) $(CFLAGS) -I$(FONTSRV) fontpipe.c

getsubfont.o: getsubfont.c
	$(CC) $(CFLAGS) -I$(LIBDRAW) getsubfont.c

# Other local sources
drawbridge.o: drawbridge.c
	$(CC) $(CFLAGS) drawbridge.c

srv-direct.o: srv-direct.c
	$(CC) $(CFLAGS) srv-direct.c

openfont.o: openfont.c
	$(CC) $(CFLAGS) openfont.c

plumb-glue.o: plumb-glue.c
	$(CC) $(CFLAGS) plumb-glue.c

plumb-rules.o: plumb-rules.c
	$(CC) $(CFLAGS) plumb-rules.c

plumb-match.o: plumb-match.c
	$(CC) $(CFLAGS) plumb-match.c

# Acme sources
$(ACME_OBJ): %.o: $(ACME)/%.c
	$(CC) $(CFLAGS) $<

# Devdraw sources
$(DEVDRAW_OBJ): %.o: $(DEVDRAW)/%.c
	$(CC) $(CFLAGS) $<

# Fontsrv sources
fontsrv-x11.o: $(FONTSRV)/x11.c
	$(CC) $(CFLAGS) -I$(FONTSRV) -o $@ $(FONTSRV)/x11.c

pjw.o: $(FONTSRV)/pjw.c
	$(CC) $(CFLAGS) -I$(FONTSRV) $(FONTSRV)/pjw.c

clean:
	rm -f *.o

.PHONY: all clean
