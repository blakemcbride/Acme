/*
 * Win command: interactive bash terminal in an acme window.
 *
 * Creates a new window, forks bash with a PTY, and bridges
 * keyboard input and PTY output.  Typed text goes into the
 * acme buffer normally; pressing Enter (or middle-clicking Send)
 * sends the pending input to bash.  PTY output is appended to
 * the body by the mousethread via flushptyout().
 */

#if defined(_WIN32) && !defined(__CYGWIN__)
/*
 * Native Windows: Win command via ConPTY (CreatePseudoConsole).
 * Requires Windows 10 1809+ (2018).
 *
 * ConPTY architecture:
 *   app -> [input pipe] -> ConPTY -> [shell stdin]
 *   app <- [output pipe] <- ConPTY <- [shell stdout/stderr]
 *
 * We wrap the input pipe's write end and the output pipe's read end
 * in POSIX file descriptors via _open_osfhandle so the rest of acme
 * can read/write the PTY using normal fd operations.
 */
/* Request Windows 10 APIs (ConPTY) before any Windows headers */
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI		/* avoid Rectangle/Polygon function conflicts */
#include <windows.h>

/* ConPTY APIs are in consoleapi.h but only with the right NTDDI_VERSION */
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif
#include <consoleapi.h>

#include <io.h>
#include <fcntl.h>

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
 * Pending PTY output (same mechanism as the POSIX build).
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

/*
 * Side-table mapping Window* -> ConPTY state.  We can't extend the
 * Window struct without affecting the POSIX build.
 */
typedef struct WinPty WinPty;
struct WinPty {
	Window	*w;
	HPCON	hpc;
	HANDLE	hProcess;
	HANDLE	hInputWrite;	/* we write here, shell reads */
	HANDLE	hOutputRead;	/* shell writes, we read */
	int	writefd;	/* fd wrapping hInputWrite */
	int	readfd;		/* fd wrapping hOutputRead */
	WinPty	*next;
};

static QLock	winptylk;
static WinPty	*winptys;

static WinPty*
winpty_get(Window *w)
{
	WinPty *p;
	qlock(&winptylk);
	for(p = winptys; p; p = p->next)
		if(p->w == w){
			qunlock(&winptylk);
			return p;
		}
	qunlock(&winptylk);
	return nil;
}

static void
winpty_add(WinPty *p)
{
	qlock(&winptylk);
	p->next = winptys;
	winptys = p;
	qunlock(&winptylk);
}

static void
winpty_remove(Window *w)
{
	WinPty **pp, *p;
	qlock(&winptylk);
	for(pp = &winptys; (p = *pp) != nil; pp = &p->next){
		if(p->w == w){
			*pp = p->next;
			free(p);
			break;
		}
	}
	qunlock(&winptylk);
}

static void
addptyout(int winid, Rune *r, int nr)
{
	Ptyout *po;

	po = emalloc(sizeof *po);
	po->winid = winid;
	po->r = r;
	po->nr = nr;
	po->next = nil;

	qlock(&ptyoutlk);
	*ptyoutstail = po;
	ptyoutstail = &po->next;
	qunlock(&ptyoutlk);

	/* Wake the main thread so it drains the queue */
	nbsendp(cptyout, nil);
}

/* Drain queued PTY output into window bodies.  Called under row.lk. */
void
flushptyout(void)
{
	Ptyout *po, *next;
	Window *w;

	qlock(&ptyoutlk);
	po = ptyouts;
	ptyouts = nil;
	ptyoutstail = &ptyouts;
	qunlock(&ptyoutlk);

	for(; po; po = next){
		next = po->next;
		w = nil;
		for(int i = 0; i < row.ncol; i++){
			Column *c = row.col[i];
			for(int j = 0; j < c->nw; j++){
				if(c->w[j]->id == po->winid){
					w = c->w[j];
					break;
				}
			}
			if(w)
				break;
		}
		if(w != nil){
			textinsert(&w->body, w->body.file->b.nc, po->r, po->nr, TRUE);
			textscrdraw(&w->body);
			textsetselect(&w->body, w->body.file->b.nc, w->body.file->b.nc);
			w->body.iq1 = w->body.file->b.nc;
			winsettag(w);
		}
		free(po->r);
		free(po);
	}
}

/*
 * Strip ANSI/VT100 control sequences that acme can't sensibly render.
 */
static int
ptyclean(char *buf, int n)
{
	char *r, *w, *e;
	r = w = buf;
	e = buf + n;
	while(r < e){
		/* CRLF -> LF; lone CR -> LF */
		if(*r == '\r'){
			r++;
			if(r < e && *r == '\n'){
				*w++ = '\n';
				r++;
			}else{
				*w++ = '\n';
			}
			continue;
		}
		/* drop BEL */
		if(*r == '\007'){
			r++;
			continue;
		}
		/* drop backspace - cmd.exe sometimes uses it */
		if(*r == '\010'){
			r++;
			continue;
		}
		if(*r == '\033'){
			r++;
			if(r >= e) continue;
			if(*r == '['){
				/* CSI: ESC [ params... letter.
				 * Some sequences imply line motion (e.g. cursor down,
				 * "go to column 1 next row").  Convert those to \n. */
				char *start = r;
				r++;
				while(r < e && !((*r >= '@' && *r <= '~')))
					r++;
				if(r < e){
					char final = *r;
					r++;
					/* H = cursor position, J = erase display.
					 * cmd.exe via ConPTY uses these instead of newlines. */
					if(final == 'H' || final == 'J' || final == 'K'){
						/* If we're not at the start of a line, add one */
						if(w > buf && *(w-1) != '\n')
							*w++ = '\n';
					}
					(void)start;
				}
			}else if(*r == ']'){
				/* OSC: ESC ] ... BEL or ESC ] ... ESC \ */
				r++;
				while(r < e && *r != '\007' && *r != '\033')
					r++;
				if(r < e && *r == '\007')
					r++;
				else if(r+1 < e && *r == '\033' && *(r+1) == '\\')
					r += 2;
			}else if(*r == '(' || *r == ')'){
				/* charset designation: ESC ( X */
				r++;
				if(r < e) r++;
			}else{
				/* single-char escape */
				r++;
			}
			continue;
		}
		*w++ = *r++;
	}
	return w - buf;
}

static void
ptyreadproc(void *v)
{
	Window *w;
	WinPty *p;
	char buf[4096];
	DWORD n;
	Rune *rp;
	int nr;

	w = v;
	p = winpty_get(w);
	if(p == nil){
		winclose(w);
		threadexits(nil);
	}

	for(;;){
		if(!ReadFile(p->hOutputRead, buf, sizeof buf, &n, nil) || n == 0)
			break;
		n = ptyclean(buf, n);
		if(n <= 0)
			continue;
		rp = runemalloc(n);
		nr = 0;
		for(int i = 0; i < (int)n; ){
			Rune r;
			int w2 = chartorune(&r, buf + i);
			rp[nr++] = r;
			i += w2;
		}
		addptyout(w->id, rp, nr);
	}
	winclose(w);
	threadexits(nil);
}

/*
 * Handle a rune typed into a Win window.  Non-printable keys that have
 * special meaning in a shell (Enter, Ctrl-C, Ctrl-D, etc.) get forwarded
 * to the shell; other keys are inserted into the buffer as normal typing.
 */
void
winptytype(Window *w, Text *t, Rune r)
{
	WinPty *p;
	char buf[8];
	int n;
	DWORD written;

	USED(t);

	if(w->ptyfd < 0)
		return;
	p = winpty_get(w);
	if(p == nil)
		return;

	/* Translate Rune to bytes */
	if(r == '\n' || r == '\r')
		n = snprint(buf, sizeof buf, "\r");
	else
		n = runetochar(buf, &r);

	WriteFile(p->hInputWrite, buf, n, &written, nil);
}

void
winptyclose(Window *w)
{
	WinPty *p;

	p = winpty_get(w);
	if(p == nil)
		return;
	if(p->hpc)
		ClosePseudoConsole(p->hpc);
	if(p->hProcess){
		TerminateProcess(p->hProcess, 1);
		CloseHandle(p->hProcess);
	}
	if(p->writefd >= 0)
		close(p->writefd);
	if(p->readfd >= 0)
		close(p->readfd);
	w->ptyfd = -1;
	w->ptypid = 0;
	winpty_remove(w);
}

void
winexec(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	Window *w;
	WinPty *p;
	Rune *rname;
	char *dir;
	int nname;
	Runestr rs;
	HANDLE hInRead = nil, hInWrite = nil;
	HANDLE hOutRead = nil, hOutWrite = nil;
	HPCON hpc = nil;
	STARTUPINFOEXA si;
	PROCESS_INFORMATION pi;
	SIZE_T attrSize = 0;
	COORD size = {80, 25};
	char cmdline[512];
	char *comspec;

	USED(_0); USED(_1); USED(_2); USED(_3); USED(_4); USED(_5);

	if(et->col == nil){
		warning(nil, "Win: no column\n");
		return;
	}

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

	/* Create the two pipes */
	if(!CreatePipe(&hInRead, &hInWrite, nil, 0)
	|| !CreatePipe(&hOutRead, &hOutWrite, nil, 0)){
		warning(nil, "Win: CreatePipe failed\n");
		goto Err;
	}

	if(CreatePseudoConsole(size, hInRead, hOutWrite, 0, &hpc) != S_OK){
		warning(nil, "Win: CreatePseudoConsole failed (requires Windows 10 1809+)\n");
		goto Err;
	}
	/* After CreatePseudoConsole, we don't need our copies of the ends
	 * the ConPTY owns (hInRead for it to read, hOutWrite for it to write). */
	CloseHandle(hInRead); hInRead = nil;
	CloseHandle(hOutWrite); hOutWrite = nil;

	/* Build attribute list for CreateProcess */
	InitializeProcThreadAttributeList(nil, 1, 0, &attrSize);
	memset(&si, 0, sizeof si);
	si.StartupInfo.cb = sizeof si;
	si.lpAttributeList = malloc(attrSize);
	if(si.lpAttributeList == nil)
		goto Err;
	if(!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)){
		free(si.lpAttributeList);
		si.lpAttributeList = nil;
		goto Err;
	}
	if(!UpdateProcThreadAttribute(si.lpAttributeList, 0,
		PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof hpc, nil, nil)){
		DeleteProcThreadAttributeList(si.lpAttributeList);
		free(si.lpAttributeList);
		si.lpAttributeList = nil;
		goto Err;
	}

	/* Command line: use COMSPEC (typically cmd.exe) */
	comspec = getenv("COMSPEC");
	if(comspec == nil || !*comspec)
		comspec = "cmd.exe";
	snprint(cmdline, sizeof cmdline, "%s", comspec);

	memset(&pi, 0, sizeof pi);
	if(!CreateProcessA(nil, cmdline, nil, nil, FALSE,
		EXTENDED_STARTUPINFO_PRESENT,
		nil, dir, &si.StartupInfo, &pi)){
		warning(nil, "Win: CreateProcess failed (%lu)\n", (ulong)GetLastError());
		DeleteProcThreadAttributeList(si.lpAttributeList);
		free(si.lpAttributeList);
		goto Err;
	}
	DeleteProcThreadAttributeList(si.lpAttributeList);
	free(si.lpAttributeList);
	CloseHandle(pi.hThread);

	/* Set up state */
	p = mallocz(sizeof *p, 1);
	p->hpc = hpc;
	p->hProcess = pi.hProcess;
	p->hInputWrite = hInWrite;
	p->hOutputRead = hOutRead;
	p->writefd = _open_osfhandle((intptr_t)hInWrite, _O_WRONLY|_O_BINARY);
	p->readfd = _open_osfhandle((intptr_t)hOutRead, _O_RDONLY|_O_BINARY);

	/* Create the window */
	w = makenewwindow(et);
	p->w = w;
	winpty_add(p);

	nname = strlen(dir) + 5;
	rname = runemalloc(nname + 1);
	runesnprint(rname, nname + 1, "%s/-win", dir);
	rs = cleanrname(runestr(rname, nname));
	winsetname(w, rs.r, rs.nr);
	free(rs.r);
	free(dir);

	w->ptyfd = p->readfd;
	w->ptypid = (int)pi.dwProcessId;
	w->isscratch = TRUE;
	w->dirty = FALSE;
	w->autoindent = FALSE;
	w->body.iq1 = 0;

	winsettag(w);
	xfidlog(w, "new");

	incref(&w->ref);
	proccreate(ptyreadproc, w, STACK);
	return;

Err:
	if(hpc) ClosePseudoConsole(hpc);
	if(hInRead) CloseHandle(hInRead);
	if(hInWrite) CloseHandle(hInWrite);
	if(hOutRead) CloseHandle(hOutRead);
	if(hOutWrite) CloseHandle(hOutWrite);
	free(dir);
}

#else /* POSIX */

/*
 * Include system PTY/terminal headers before Plan 9 headers
 * to avoid wait/waitpid macro conflicts.
 */
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
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
		if(shell == nil || shell[0] == 0)
			shell = "/bin/bash";
		/* Try bash with minimal startup; fall back to plain interactive */
		execl(shell, shell, "--noediting", "--norc", "--noprofile", "-i", nil);
		execl(shell, shell, "-i", nil);
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

#endif /* !_WIN32 */
