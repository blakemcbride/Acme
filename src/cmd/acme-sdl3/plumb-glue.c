/*
 * Embedded plumber: glue code.
 * Provides globals, initialization, and renamed utility functions
 * that would conflict with acme's own symbols.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <regexp.h>
#include <thread.h>
#include <plumb.h>
#include "plumb-internal.h"

int plumb_debug;
int plumb_nports;
char **plumb_ports;
Ruleset **plumb_rules;
char *plumb_user;
char *plumb_home;
jmp_buf plumb_parsejmp;
char *plumb_lasterror;

static int plumb_ready;

void
plumb_makeports(Ruleset *rules[])
{
	int i;

	for(i=0; rules[i]; i++)
		plumb_addport(rules[i]->port);
}

void
plumb_addport(char *port)
{
	int i;

	if(port == nil)
		return;
	for(i=0; i<plumb_nports; i++)
		if(strcmp(plumb_ports[i], port) == 0)
			return;
	plumb_ports = plumb_erealloc(plumb_ports, (plumb_nports+1)*sizeof(char*));
	plumb_ports[plumb_nports++] = plumb_estrdup(port);
}

void
plumb_error(char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vseprint(buf, buf+sizeof buf, fmt, args);
	va_end(args);

	fprint(2, "plumber: %s\n", buf);
	/* In embedded mode, don't exit -- just log the error */
}

void
plumb_parseerror(char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vseprint(buf, buf+sizeof buf, fmt, args);
	va_end(args);

	plumb_printinputstack();
	fprint(2, "%s\n", buf);
	do; while(plumb_popinput());
	plumb_lasterror = plumb_estrdup(buf);
	longjmp(plumb_parsejmp, 1);
}

void*
plumb_emalloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == nil){
		fprint(2, "plumber: malloc failed: %r\n");
		return nil;
	}
	memset(p, 0, n);
	return p;
}

void*
plumb_erealloc(void *p, long n)
{
	p = realloc(p, n);
	if(p == nil)
		fprint(2, "plumber: realloc failed: %r\n");
	return p;
}

char*
plumb_estrdup(char *s)
{
	char *t;

	t = strdup(s);
	if(t == nil)
		fprint(2, "plumber: estrdup failed: %r\n");
	return t;
}

/*
 * Built-in plumbing rules, used when ~/lib/plumbing doesn't exist.
 * This is fileaddr + basic inlined, with Plan 9 tools replaced by
 * standard platform equivalents (xdg-open on Linux, open on macOS).
 */

#if defined(__APPLE__)
#define PLUMB_OPENER "open"
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define PLUMB_OPENER "cygstart"
#else
#define PLUMB_OPENER "xdg-open"
#endif

static char defaultrules[] =
	"# address patterns\n"
	"addrelem='((#?[0-9]+)|(/[A-Za-z0-9_\\^]+/?)|[.$])'\n"
	"addr=:($addrelem([,;+\\-]$addrelem)*)\n"
	"twocolonaddr = ([0-9]+)[:.]([0-9]+)\n"
	"\n"
	"editor = acme\n"
	"\n"
	"# urls go to web browser\n"
	"type is text\n"
	"data matches '(https?|ftp|file|gopher|mailto|news|nntp|telnet|wais|prospero)://[a-zA-Z0-9_@\\-]+([.:][a-zA-Z0-9_@\\-]+)*/?[a-zA-Z0-9_?,%#~&/\\-+=]+([:.][a-zA-Z0-9_?,%#~&/\\-+=]+)*'\n"
	"plumb to web\n"
	"plumb start " PLUMB_OPENER " $0\n"
	"\n"
	"# image files\n"
	"type is text\n"
	"data matches '[a-zA-Z\302-\357\240-\277\200-\2770-9_\\-./@]+'\n"
	"data matches '([a-zA-Z\302-\357\240-\277\200-\2770-9_\\-./@]+)\\.(jpe?g|JPE?G|gif|GIF|tiff?|TIFF?|ppm|bit|png|PNG)'\n"
	"arg isfile	$0\n"
	"plumb to image\n"
	"plumb start " PLUMB_OPENER " $file\n"
	"\n"
	"# pdf/ps/dvi files\n"
	"type is text\n"
	"data matches '[a-zA-Z\302-\357\240-\277\200-\2770-9_\\-./@]+'\n"
	"data matches '([a-zA-Z\302-\357\240-\277\200-\2770-9_\\-./@]+)\\.(ps|PS|eps|EPS|pdf|PDF|dvi|DVI)'\n"
	"arg isfile	$0\n"
	"plumb to postscript\n"
	"plumb start " PLUMB_OPENER " $file\n"
	"\n"
	"# existing files tagged by line:col twice, go to editor\n"
	"type is text\n"
	"data matches '([.a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-@]*[a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-])':$twocolonaddr,$twocolonaddr\n"
	"arg isfile     $1\n"
	"data set       $file\n"
	"attr add       addr=$2-#0+#$3-#1,$4-#0+#$5-#1\n"
	"plumb to edit\n"
	"plumb client $editor\n"
	"\n"
	"# existing files tagged by line:col, go to editor\n"
	"type is text\n"
	"data matches '([.a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-@]*[a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-])':$twocolonaddr\n"
	"arg isfile     $1\n"
	"data set       $file\n"
	"attr add       addr=$2-#0+#$3-#1\n"
	"plumb to edit\n"
	"plumb client $editor\n"
	"\n"
	"# existing files, possibly tagged by line number, go to editor\n"
	"type is text\n"
	"data matches '([.a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-@]*[a-zA-Z\302-\357\240-\277\200-\2770-9_/\\-])'($addr)?\n"
	"arg isfile	$1\n"
	"data set	$file\n"
	"attr add	addr=$3\n"
	"plumb to edit\n"
	"plumb client $editor\n"
	"\n"
	"# .h files in /usr/include\n"
	"type is text\n"
	"data matches '([a-zA-Z\302-\357\240-\277\200-\2770-9/_\\-]+\\.h)'($addr)?\n"
	"arg isfile	/usr/include/$1\n"
	"data set	$file\n"
	"attr add	addr=$3\n"
	"plumb to edit\n"
	"plumb client $editor\n"
	"\n"
	"# .h files in /usr/local/include\n"
	"type is text\n"
	"data matches '([a-zA-Z\302-\357\240-\277\200-\2770-9/_\\-]+\\.h)'($addr)?\n"
	"arg isfile	/usr/local/include/$1\n"
	"data set	$file\n"
	"attr add	addr=$3\n"
	"plumb to edit\n"
	"plumb client $editor\n"
	"\n"
;

static void
writeproc(void *v)
{
	int *fdp;

	fdp = v;
	write(fdp[1], defaultrules, strlen(defaultrules));
	close(fdp[1]);
}

/*
 * Initialize the embedded plumber.
 * Load rules from ~/lib/plumbing, or use built-in defaults.
 * Returns 0 on success, -1 on failure.
 */
int
plumb_init(void)
{
	char buf[512];
	char *plumbfile;
	int fd;
	int p[2];

	if(plumb_ready)
		return 0;

	plumb_user = getuser();
	plumb_home = getenv("HOME");
#if defined(_WIN32) && !defined(__CYGWIN__)
	if(plumb_home == nil)
		plumb_home = getenv("USERPROFILE");
	if(plumb_home == nil)
		plumb_home = "C:";
	if(plumb_user == nil)
		plumb_user = "user";
#endif
	if(plumb_user==nil || plumb_home==nil){
		fprint(2, "plumber: can't initialize $user or $home: %r\n");
		return -1;
	}

	plumbfile = nil;
	fd = -1;
	sprint(buf, "%s/lib/plumbing", plumb_home);
	if(access(buf, 0) >= 0){
		plumbfile = plumb_estrdup(buf);
		fd = open(plumbfile, OREAD);
	}

	if(fd < 0){
		/* Use built-in default rules via pipe */
		if(pipe(p) < 0){
			fprint(2, "plumber: can't create pipe for default rules: %r\n");
			return -1;
		}
		plumbfile = plumb_estrdup("<built-in>");
		fd = p[0];
		proccreate(writeproc, p, 32*1024);
	}

	if(setjmp(plumb_parsejmp)){
		fprint(2, "plumber: parse error in %s\n", plumbfile);
		close(fd);
		free(plumbfile);
		return -1;
	}

	plumb_rules = plumb_readrules(plumbfile, fd);
	close(fd);
	free(plumbfile);

	plumb_makeports(plumb_rules);
	plumb_ready = 1;
	return 0;
}

/*
 * Try to match a plumb message against the loaded rules.
 * Returns:
 *   0 = matched and delivered (edit port handled by caller)
 *   1 = matched with start action
 *  -1 = no match
 *
 * If matched, msg->dst is set to the destination port.
 * The Exec* (if non-nil) should be freed by caller with plumb_freeexec.
 */
int
plumb_send(Plumbmsg *m)
{
	int i;
	Exec *e;
	char *err;

	if(plumb_rules == nil)
		return -1;

	for(i=0; plumb_rules[i]; i++){
		e = plumb_matchruleset(m, plumb_rules[i]);
		if(e != nil){
			if(m->dst != nil && strcmp(m->dst, "edit") == 0){
				/* "to edit" -- caller handles delivery */
				plumb_freeexec(e);
				return 0;
			}
			/* has a start or client action */
			err = plumb_startup(plumb_rules[i], e);
			if(err != nil)
				fprint(2, "plumber: %s\n", err);
			plumb_freeexec(e);
			return 1;
		}
	}
	return -1;
}
