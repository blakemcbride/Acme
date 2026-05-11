override PLAN9 := $(realpath .)
export PLAN9

# On MSYS2, pin the toolchain gcc that matches the active subsystem so
# every library is built with the same ABI as the final acme binary.
# Defaults to /ucrt64 but honors $MSYSTEM_PREFIX (MINGW64 / CLANG64 work
# transparently).  Override on the command line if needed:
#   make MSYS_PREFIX=/mingw64
ifneq (,$(findstring NT,$(shell uname -s)))
MSYS_PREFIX ?= $(if $(MSYSTEM_PREFIX),$(MSYSTEM_PREFIX),/ucrt64)
CC := $(MSYS_PREFIX)/bin/gcc
export CC
export MSYS_PREFIX
endif

LIBS = lib9 libbio libregexp libthread libmux lib9pclient libauth \
	libauthsrv libip libndb libsec libmp libcomplete libplumb \
	libdraw libmemdraw libmemlayer libframe

all: libs
	$(MAKE) -C src/cmd/acme-sdl3

libs:
	@for d in $(LIBS); do \
		$(MAKE) -C src/$$d || exit 1; \
	done

clean:
	@for d in $(LIBS); do \
		$(MAKE) -C src/$$d clean; \
	done
	$(MAKE) -C src/cmd/acme-sdl3 clean
	find src -name '*.o' -delete
	rm -f bin/acme bin/acme.exe

realclean: clean
	rm -f lib/*.a

.PHONY: all libs clean realclean
