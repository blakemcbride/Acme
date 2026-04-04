LIBNAME = libip.a

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PLATFORM_SRC = Darwin.c
else ifneq (,$(findstring NT,$(UNAME_S)))
PLATFORM_SRC = none.c
else
PLATFORM_SRC = Linux.c
endif

SRCS = bo.c classmask.c eipfmt.c freeipifc.c ipaux.c myetheraddr.c \
	myipaddr.c parseether.c parseip.c ptclbsum.c udp.c $(PLATFORM_SRC)
include ../mklib.mk
