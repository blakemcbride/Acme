/*
 * Win command: interactive bash terminal in an acme window.
 *
 * Creates a new window, forks bash with a PTY, and bridges
 * keyboard input and PTY output.  Typed text goes into the
 * acme buffer normally; pressing Enter (or middle-clicking Send)
 * sends the pending input to bash.  PTY output is appended to
 * the body by the mousethread via flushptyout().
 */

/*
 * Include system PTY/terminal headers before Plan 9 headers
 * to avoid wait/waitpid macro conflicts.
 */
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>

/* openpty prototype - from pty.h but that header pulls in sys/wait.h */
extern int openpty(int *, int *, char *, struct termios *, struct winsize *);

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

/*
 * Pending PTY output: a linked list of buffers, flushed into
 * the window body by flushptyout() under row.lk.
 */
typedef struct Ptyout Ptyout;
struct Ptyout
{
	int	winid;
	Rune	*r;
	int	nr;
	Ptyout	*next;
};

static QLock	ptyoutlk;
static Ptyout	*ptyouts;
static Ptyout	**ptyoutstail = &ptyouts;

static void
addptyout(int winid, Rune *r, int nr)
{
	Ptyout *p;

	p = emalloc(sizeof(Ptyout));
	p->winid = winid;
	p->r = runemalloc(nr);
	runemove(p->r, r, nr);
	p->nr = nr;
	p->next = nil;
	qlock(&ptyoutlk);
	*ptyoutstail = p;
	ptyoutstail = &p->next;
	qunlock(&ptyoutlk);
	nbsendp(cptyout, nil);
}

/*
 * Called from mousethread with row.lk held.
 * Flushes all pending PTY output into the respective windows.
 */
void
flushptyout(void)
{
	Ptyout *p, *next, *list;
	Window *w;
	Text *t;
	int owner;

	qlock(&ptyoutlk);
	list = ptyouts;
	ptyouts = nil;
	ptyoutstail = &ptyouts;
	qunlock(&ptyoutlk);

	for(p = list; p; p = next){
		next = p->next;
		w = lookid(p->winid, 0);
		if(w != nil){
			winlock(w, 'F');
			t = &w->body;
			owner = w->owner;
			if(owner == 0)
				w->owner = 'W';
			wincommit(w, t);
			textinsert(t, t->file->b.nc, p->r, p->nr, TRUE);
			w->ptyboundary = t->file->b.nc;
			textshow(t, t->file->b.nc, t->file->b.nc, 1);
			textscrdraw(t);
			w->dirty = FALSE;
			w->owner = owner;
			winunlock(w);
		}
		free(p->r);
		free(p);
	}
}

/*
 * Reader proc: reads PTY output and queues it for the main thread.
 */
/*
 * Strip \r and ANSI/OSC escape sequences from PTY output.
 * Returns new length.
 */
static int
ptyclean(char *buf, int n)
{
	int i, j;

	j = 0;
	for(i = 0; i < n; i++){
		if(buf[i] == '\r')
			continue;
		/* ESC [ ... final_byte  (CSI sequences) */
		if(buf[i] == '\033' && i+1 < n && buf[i+1] == '['){
			i += 2;
			while(i < n && (buf[i] < 0x40 || buf[i] > 0x7E))
				i++;
			/* i now points at final byte; loop increment skips it */
			continue;
		}
		/* ESC ] ... BEL  or  ESC ] ... ESC \  (OSC sequences) */
		if(buf[i] == '\033' && i+1 < n && buf[i+1] == ']'){
			i += 2;
			while(i < n){
				if(buf[i] == '\007')	/* BEL terminates OSC */
					break;
				if(buf[i] == '\033' && i+1 < n && buf[i+1] == '\\'){
					i++;	/* skip the backslash too */
					break;
				}
				i++;
			}
			continue;
		}
		/* Bare ESC followed by single character (e.g. ESC =, ESC >) */
		if(buf[i] == '\033' && i+1 < n && buf[i+1] != '[' && buf[i+1] != ']'){
			i++;
			continue;
		}
		buf[j++] = buf[i];
	}
	return j;
}

static void
ptyreadproc(void *v)
{
	Window *w;
	int fd, n, nr, nulls, winid;
	char buf[8192];
	Rune rbuf[8192];

	w = v;
	fd = w->ptyfd;
	winid = w->id;

	threadsetname("ptyreadproc");
	for(;;){
		n = read(fd, buf, sizeof(buf)-1);
		if(n <= 0)
			break;
		n = ptyclean(buf, n);
		if(n == 0)
			continue;
		buf[n] = 0;
		cvttorunes(buf, n, rbuf, &n, &nr, &nulls);
		if(nr > 0)
			addptyout(winid, rbuf, nr);
	}
	/* PTY closed (shell exited); nothing more to do.
	 * The window remains; user can Del it. */
}

/*
 * Handle keystrokes in a Win window body.
 * Types text into the buffer normally, but on Enter,
 * sends the pending input (from iq1 to end) to the PTY.
 */
void
winptytype(Window *w, Text *t, Rune r)
{
	int i;
	uint q0, q1, n;
	Rune *rp;
	char *s;

	/* Let acme insert the character normally */
	texttype(t, r);
	if(t->what == Body)
		for(i=0; i<t->file->ntext; i++)
			textscrdraw(t->file->text[i]);
	winsettag(w);

	/* On newline, send pending input to PTY */
	if(r == '\n' && w->ptyfd >= 0){
		q0 = w->ptyboundary;
		q1 = t->file->b.nc;
		if(q1 > q0){
			n = q1 - q0;
			rp = runemalloc(n);
			bufread(&t->file->b, q0, rp, n);
			s = runetobyte(rp, n);
			write(w->ptyfd, s, strlen(s));
			free(s);
			free(rp);
		}
		w->ptyboundary = t->file->b.nc;
	}
}

/*
 * Clean up PTY and shell process.
 */
void
winptyclose(Window *w)
{
	if(w->ptyfd >= 0){
		close(w->ptyfd);
		w->ptyfd = -1;
	}
	if(w->ptypid > 0){
		kill(w->ptypid, SIGHUP);
		w->ptypid = 0;
	}
}

/*
 * Win command: create a new window with an interactive bash session.
 */
void
winexec(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	Window *w;
	int master, slave;
	pid_t pid;
	Rune *rname;
	char *dir, *shell;
	int nname;
	Runestr rs;
	struct termios term;
	struct winsize wsz;

	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	if(et->col == nil){
		warning(nil, "Win: no column\n");
		return;
	}

	/* Determine directory from current window */
	rs = dirname(et, nil, 0);
	if(rs.nr == 1 && rs.r[0] == '.'){
		free(rs.r);
		rs.r = nil;
		rs.nr = 0;
	}
	if(rs.r != nil){
		dir = runetobyte(rs.r, rs.nr);
		free(rs.r);
	}else
		dir = estrdup(wdir);

	/* Open PTY */
	if(openpty(&master, &slave, nil, nil, nil) < 0){
		warning(nil, "Win: openpty failed: %r\n");
		free(dir);
		return;
	}

	/* Set PTY to 1 column wide: forces ls and similar to single-column output.
	 * Acme is a text editor, not a terminal; column formatting just makes a mess. */
	memset(&wsz, 0, sizeof wsz);
	wsz.ws_col = 1;
	wsz.ws_row = 24;
	ioctl(master, TIOCSWINSZ, &wsz);

	/* Configure PTY: disable echo since acme handles display */
	if(tcgetattr(slave, &term) == 0){
		term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
		tcsetattr(slave, TCSANOW, &term);
	}

	pid = fork();
	if(pid < 0){
		warning(nil, "Win: fork failed: %r\n");
		close(master);
		close(slave);
		free(dir);
		return;
	}
	if(pid == 0){
		/* Child: set up PTY slave as stdin/stdout/stderr */
		close(master);
		setsid();
		ioctl(slave, TIOCSCTTY, 0);
		dup2(slave, 0);
		dup2(slave, 1);
		dup2(slave, 2);
		if(slave > 2)
			close(slave);
		chdir(dir);

		/* Dumb terminal: suppress ANSI escapes, color prompts, VTE hooks */
		setenv("TERM", "dumb", 1);
		unsetenv("PROMPT_COMMAND");
		unsetenv("VTE_VERSION");
		unsetenv("COLORTERM");
		unsetenv("LS_COLORS");
		setenv("PS1", "$ ", 1);

		shell = getenv("SHELL");
		if(shell == nil)
			shell = "/bin/bash";
		execl(shell, shell, "--noediting", "--norc", "--noprofile", "-i", nil);
		execl("/bin/sh", "sh", "-i", nil);
		_exit(1);
	}

	/* Parent */
	close(slave);

	/* Create the window */
	w = makenewwindow(et);
	nname = strlen(dir) + 5;
	rname = runemalloc(nname + 1);
	runesnprint(rname, nname + 1, "%s/-win", dir);
	rs = cleanrname(runestr(rname, nname));
	winsetname(w, rs.r, rs.nr);
	free(rs.r);
	free(dir);

	w->ptyfd = master;
	w->ptypid = pid;
	w->isscratch = TRUE;
	w->dirty = FALSE;
	w->autoindent = FALSE;
	w->body.iq1 = 0;

	winsettag(w);
	xfidlog(w, "new");

	/* Start reader thread */
	incref(&w->ref);
	proccreate(ptyreadproc, w, STACK);
}
