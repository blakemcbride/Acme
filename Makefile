override PLAN9 := $(realpath .)
export PLAN9

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

.PHONY: all libs clean realclean
