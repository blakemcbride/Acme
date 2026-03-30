LIBNAME = libmp.a

PORT_SRCS = mpaux.c mpfmt.c strtomp.c mptobe.c mptole.c betomp.c letomp.c \
	mpadd.c mpsub.c mpcmp.c mpfactorial.c mpmul.c mpleft.c mpright.c \
	mpvecadd.c mpvecsub.c mpvecdigmuladd.c mpveccmp.c mpdigdiv.c \
	mpdiv.c mpexp.c mpmod.c mpextendedgcd.c mpinvert.c mprand.c crt.c \
	mptoi.c mptoui.c mptov.c mptouv.c

SRCS = $(addprefix port/,$(PORT_SRCS))
include ../mklib.mk
