# Common rules for building Plan 9 static libraries with GNU make.
# Each library Makefile defines LIBNAME, SRCS (and optionally EXTRA_CFLAGS),
# then includes this file.

PLAN9 ?= $(realpath ../..)
UNAME_S := $(shell uname -s)
CC ?= cc
AR = ar
ARFLAGS = rcs

# GCC-only warning flags (clang does not support these)
GCC_WARN_FLAGS = \
	-Wno-array-parameter \
	-Wno-stringop-truncation \
	-Wno-stringop-overflow \
	-Wno-format-truncation \
	-Wno-unused-but-set-variable

ifneq ($(UNAME_S),Darwin)
PLATFORM_WARN_FLAGS = $(GCC_WARN_FLAGS)
endif

CFLAGS = -O2 -c \
	-I$(PLAN9)/include \
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
	-DPLAN9PORT \
	$(EXTRA_CFLAGS)

TARGET = $(PLAN9)/lib/$(LIBNAME)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS)

.PHONY: all clean
