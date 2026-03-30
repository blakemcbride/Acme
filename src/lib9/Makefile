PLAN9 ?= $(realpath ../..)
LIBNAME = lib9.a

LIB9_SRCS = _exits.c _p9dialparse.c _p9dir.c announce.c argv0.c atexit.c \
	atoi.c atol.c atoll.c atnotify.c await.c cistrcmp.c cistrncmp.c \
	cistrstr.c cleanname.c convD2M.c convM2D.c convM2S.c convS2M.c \
	crypt.c ctime.c dial.c dirfstat.c dirfwstat.c dirmodefmt.c dirstat.c \
	dirwstat.c dup.c encodefmt.c errstr.c exec.c execl.c exitcode.c \
	fcallfmt.c frand.c get9root.c getcallerpc.c getenv.c getfields.c \
	getnetconn.c getns.c getuser.c getwd.c jmp.c lrand.c lnrand.c \
	main.c malloc.c malloctag.c mallocz.c nan.c needsrcquote.c \
	needstack.c netcrypt.c netmkaddr.c notify.c nrand.c nulldir.c \
	open.c opentemp.c pin.c pipe.c post9p.c postnote.c qlock.c quote.c \
	rand.c read9pmsg.c readcons.c readn.c rfork.c searchpath.c sendfd.c \
	sleep.c strdup.c strecpy.c sysfatal.c syslog.c sysname.c time.c \
	tm2sec.c tokenize.c truerand.c u16.c u32.c u64.c unsharp.c wait.c \
	waitpid.c write.c zoneinfo.c fmtlock2.c frexp.c

FMT_SRCS = dofmt.c fltfmt.c fmt.c fmtfd.c fmtfdflush.c fmtlocale.c \
	fmtnull.c fmtprint.c fmtquote.c fmtrune.c fmtstr.c \
	fmtvprint.c fprint.c nan64.c print.c runefmtstr.c \
	runeseprint.c runesmprint.c runesnprint.c runesprint.c \
	runevseprint.c runevsmprint.c runevsnprint.c seprint.c smprint.c \
	snprint.c sprint.c strtod.c vfprint.c vseprint.c vsmprint.c \
	vsnprint.c charstod.c pow10.c

UTF_SRCS = rune.c runestrcat.c runestrchr.c runestrcmp.c runestrcpy.c \
	runestrdup.c runestrlen.c runestrecpy.c runestrncat.c runestrncmp.c \
	runestrncpy.c runestrrchr.c runestrstr.c runetype.c utfecpy.c \
	utflen.c utfnlen.c utfrrune.c utfrune.c utfutf.c

SRCS = $(LIB9_SRCS) $(addprefix fmt/,$(FMT_SRCS)) $(addprefix utf/,$(UTF_SRCS))

include ../mklib.mk

# Override: fmt/ files need -Ifmt
fmt/%.o: fmt/%.c
	$(CC) $(CFLAGS) -Ifmt -o $@ $<

# get9root needs PLAN9_TARGET
get9root.o: get9root.c
	$(CC) $(CFLAGS) -DPLAN9_TARGET=\"$(PLAN9)\" -o $@ $<
