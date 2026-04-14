override PLAN9 := $(realpath .)
export PLAN9

# On MSYS2/Cygwin, pin the MSYS gcc so that installing mingw64-gcc
# (needed only for the FreeType ABI shim) does not hijack CC.
ifneq (,$(findstring NT,$(shell uname -s)))
CC := /usr/bin/gcc
export CC
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
	rm -f bin/acme

realclean: clean
	rm -f lib/*.a

# Native Windows build: standalone .exe with no Cygwin/MSYS runtime dependency.
# Uses mingw64 gcc with static linking.
native-win:
	$(MAKE) NATIVE_WIN=1 CC=/mingw64/bin/gcc all

.PHONY: all libs clean realclean native-win
