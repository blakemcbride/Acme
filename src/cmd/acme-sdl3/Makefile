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
UNAME_S := $(shell uname -s)

CC ?= cc

# GCC-only warning flags (clang does not support these)
GCC_WARN_FLAGS = \
	-Wno-array-parameter \
	-Wno-stringop-truncation \
	-Wno-stringop-overflow \
	-Wno-format-truncation \
	-Wno-unused-but-set-variable

ifeq ($(UNAME_S),Darwin)
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null || echo "-I/opt/homebrew/include/freetype2 -I/opt/homebrew/include")
FREETYPE_LDFLAGS := $(shell pkg-config --libs freetype2 2>/dev/null || echo "-L/opt/homebrew/lib -lfreetype")
SDL3_CFLAGS := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LDFLAGS := $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")
FONTCONFIG_CFLAGS := $(shell pkg-config --cflags fontconfig 2>/dev/null)
FONTCONFIG_LDFLAGS := $(shell pkg-config --libs fontconfig 2>/dev/null || echo "-lfontconfig")
PLATFORM_WARN_FLAGS =
PLATFORM_LDFLAGS = -lm -lpthread -framework CoreFoundation
else ifneq (,$(findstring NT,$(UNAME_S)))
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null)
FREETYPE_LDFLAGS := $(shell pkg-config --libs freetype2 2>/dev/null || echo "-lfreetype")
SDL3_CFLAGS := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LDFLAGS := $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")
FONTCONFIG_CFLAGS := $(shell pkg-config --cflags fontconfig 2>/dev/null)
FONTCONFIG_LDFLAGS := $(shell pkg-config --libs fontconfig 2>/dev/null || echo "-lfontconfig")
PLATFORM_WARN_FLAGS = $(GCC_WARN_FLAGS)
PLATFORM_LDFLAGS = -lm -lpthread -lws2_32
else
FREETYPE_CFLAGS = -I/usr/include/freetype2
FREETYPE_LDFLAGS = -lfreetype
SDL3_CFLAGS =
SDL3_LDFLAGS = -lSDL3
FONTCONFIG_CFLAGS =
FONTCONFIG_LDFLAGS = -lfontconfig
PLATFORM_WARN_FLAGS = $(GCC_WARN_FLAGS)
PLATFORM_LDFLAGS = -lm -lutil -lresolv -lpthread
endif

CFLAGS = -O2 -c \
	-I$(PLAN9)/include \
	$(FREETYPE_CFLAGS) \
	$(SDL3_CFLAGS) \
	$(FONTCONFIG_CFLAGS) \
	-Wall \
	$(PLATFORM_WARN_FLAGS) \
	-Wno-parentheses \
	-Wno-missing-braces \
	-Wno-switch \
	-Wno-comment \
	-Wno-sign-compare \
	-Wno-unknown-pragmas \
	-Wno-misleading-indentation \
	-Wno-deprecated-declarations \
	-fno-omit-frame-pointer \
	-fsigned-char \
	-fno-common \
	-std=gnu11 \
	-ggdb

LDFLAGS = -L$(PLAN9)/lib \
	$(SDL3_LDFLAGS) $(FONTCONFIG_LDFLAGS) $(FREETYPE_LDFLAGS) -lz \
	-lsec -lauth -lauthsrv -lmemdraw -lndb -lplumb \
	-lmemlayer -lframe -ldraw -l9pclient -lip -lmp -lmux \
	-lthread -lregexp9 -lcomplete -lbio -l9 -lthread -l9 -lsec -l9 \
	$(PLATFORM_LDFLAGS)

TARGET = $(PLAN9)/bin/acme

# Source directories
ACME = ../acme
DEVDRAW = ../devdraw
FONTSRV = ../fontsrv
LIBDRAW = ../../libdraw

# Local sources (in this directory)
LOCAL_SRC = drawbridge.c srv-direct.c look.c fsys.c \
	fontpipe.c openfont.c getsubfont.c \
	plumb-glue.c plumb-rules.c plumb-match.c win.c pipecmd.c

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

win.o: win.c
	$(CC) $(CFLAGS) -I$(ACME) win.c

pipecmd.o: pipecmd.c
	$(CC) $(CFLAGS) -I$(ACME) pipecmd.c

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
