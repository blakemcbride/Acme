#include "threadimpl.h"

static Lock thewaitlock;
static Channel *thewaitchan;

static void
execproc(void *v)
{
	int pid;
	Channel *c;
	Execjob *e;
	Waitmsg *w;

	e = v;
	pid = _threadspawn(e->fd, e->cmd, e->argv, e->dir);
	sendul(e->c, pid);
	if(pid > 0){
		w = waitfor(pid);
		if((c = thewaitchan) != nil)
			sendp(c, w);
		else
			free(w);
	}
	threadexits(nil);
}

int
_runthreadspawn(int *fd, char *cmd, char **argv, char *dir)
{
	int i, pid, argc;
	Execjob *e;

	// Copy all args into malloc'ed memory, in case the arguments
	// point into a thread stack. On Solaris, the stacks of other
	// threads are not inherited by the forked child.
	e = mallocz(sizeof *e, 1);
	memmove(e->fd, fd, 3*sizeof fd[0]);
	e->cmd = strdup(cmd);
	if(e->cmd == nil)
		sysfatal("out of memory");
	argc = 0;
	while(argv[argc] != nil)
		argc++;
	e->argv = mallocz((argc+1)*sizeof argv[0], 1);
	if(e->argv == nil)
		sysfatal("out of memory");
	for(i=0; i<argc; i++) {
		e->argv[i] = strdup(argv[i]);
		if(e->argv[i] == nil)
			sysfatal("out of memory");
	}
	if(dir != nil) {
		e->dir = strdup(dir);
		if(e->dir == nil)
			sysfatal("out of memory");
	}
	e->c = chancreate(sizeof(void*), 0);

	proccreate(execproc, e, 65536);
	pid = recvul(e->c);

	free(e->cmd);
	for(i=0; i<argc; i++)
		free(e->argv[i]);
	free(e->argv);
	free(e->dir);
	chanfree(e->c);
	free(e);
	return pid;
}

Channel*
threadwaitchan(void)
{
	if(thewaitchan)
		return thewaitchan;
	lock(&thewaitlock);
	if(thewaitchan){
		unlock(&thewaitlock);
		return thewaitchan;
	}
	thewaitchan = chancreate(sizeof(Waitmsg*), 4);
	chansetname(thewaitchan, "threadwaitchan");
	unlock(&thewaitlock);
	return thewaitchan;
}

#if defined(_WIN32) && !defined(__CYGWIN__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * Build a Windows command line string from argv[].
 * Caller must free the result.
 */
static char*
buildcmdline(char *argv[])
{
	int i, n;
	char *s, *p;

	n = 0;
	for(i=0; argv[i]; i++)
		n += strlen(argv[i]) + 3; /* quotes + space */
	s = malloc(n+1);
	if(s == nil)
		return nil;
	p = s;
	for(i=0; argv[i]; i++){
		if(i > 0)
			*p++ = ' ';
		/* simple quoting: wrap in double quotes if contains space */
		if(strchr(argv[i], ' ') || strchr(argv[i], '\t')){
			*p++ = '"';
			memmove(p, argv[i], strlen(argv[i]));
			p += strlen(argv[i]);
			*p++ = '"';
		}else{
			memmove(p, argv[i], strlen(argv[i]));
			p += strlen(argv[i]);
		}
	}
	*p = '\0';
	return s;
}

/*
 * Process handle table: maps Windows PIDs to HANDLEs
 * so that await/waitfor can use WaitForSingleObject.
 */
#define MAX_CHILD_PROCS 256
static struct {
	Lock lk;
	struct { DWORD pid; HANDLE h; } entries[MAX_CHILD_PROCS];
	int n;
} proctab;

void
_threadaddproc(DWORD pid, HANDLE h)
{
	lock(&proctab.lk);
	if(proctab.n < MAX_CHILD_PROCS){
		proctab.entries[proctab.n].pid = pid;
		proctab.entries[proctab.n].h = h;
		proctab.n++;
	}
	unlock(&proctab.lk);
}

HANDLE
_threadprochandle(int pid)
{
	int i;
	HANDLE h = INVALID_HANDLE_VALUE;

	lock(&proctab.lk);
	for(i=0; i<proctab.n; i++){
		if(proctab.entries[i].pid == (DWORD)pid){
			h = proctab.entries[i].h;
			/* remove entry */
			proctab.entries[i] = proctab.entries[--proctab.n];
			break;
		}
	}
	unlock(&proctab.lk);
	return h;
}

int
_threadspawn(int fd[3], char *cmd, char *argv[], char *dir)
{
	char *cmdline;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	HANDLE hStdin, hStdout, hStderr;
	BOOL ok;

	cmdline = buildcmdline(argv);
	if(cmdline == nil)
		return -1;

	memset(&si, 0, sizeof si);
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESTDHANDLES;

	hStdin = (HANDLE)_get_osfhandle(fd[0]);
	hStdout = (HANDLE)_get_osfhandle(fd[1]);
	hStderr = (HANDLE)_get_osfhandle(fd[2]);

	/* ensure handles are inheritable */
	SetHandleInformation(hStdin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
	SetHandleInformation(hStdout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
	SetHandleInformation(hStderr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

	si.hStdInput = hStdin;
	si.hStdOutput = hStdout;
	si.hStdError = hStderr;

	ok = CreateProcessA(
		nil,		/* application name (use cmdline) */
		cmdline,	/* command line */
		nil,		/* process security */
		nil,		/* thread security */
		TRUE,		/* inherit handles */
		CREATE_NEW_PROCESS_GROUP,
		nil,		/* environment (inherit) */
		dir,		/* working directory */
		&si,
		&pi);

	free(cmdline);

	if(!ok)
		return -1;

	CloseHandle(pi.hThread);
	_threadaddproc(pi.dwProcessId, pi.hProcess);

	close(fd[0]);
	if(fd[1] != fd[0])
		close(fd[1]);
	if(fd[2] != fd[1] && fd[2] != fd[0])
		close(fd[2]);

	return (int)pi.dwProcessId;
}

#else /* POSIX */

int
_threadspawn(int fd[3], char *cmd, char *argv[], char *dir)
{
	int i, n, p[2], pid;
	char exitstr[100];

	notifyoff("sys: child");	/* do not let child note kill us */
	if(pipe(p) < 0)
		return -1;
	if(fcntl(p[0], F_SETFD, 1) < 0 || fcntl(p[1], F_SETFD, 1) < 0){
		close(p[0]);
		close(p[1]);
		return -1;
	}
	switch(pid = fork()){
	case -1:
		close(p[0]);
		close(p[1]);
		return -1;
	case 0:
		/* can't RFNOTEG - will lose tty */
		if(dir != nil)
			chdir(dir); /* best effort */
		dup2(fd[0], 0);
		dup2(fd[1], 1);
		dup2(fd[2], 2);
		if(!isatty(0) && !isatty(1) && !isatty(2))
			rfork(RFNOTEG);
		for(i=3; i<100; i++)
			if(i != p[1])
				close(i);
		execvp(cmd, argv);
		fprint(p[1], "%d", errno);
		close(p[1]);
		_exit(0);
	}

	close(p[1]);
	n = read(p[0], exitstr, sizeof exitstr-1);
	close(p[0]);
	if(n > 0){	/* exec failed */
		free(waitfor(pid));
		exitstr[n] = 0;
		errno = atoi(exitstr);
		return -1;
	}

	close(fd[0]);
	if(fd[1] != fd[0])
		close(fd[1]);
	if(fd[2] != fd[1] && fd[2] != fd[0])
		close(fd[2]);
	return pid;
}

#endif /* _WIN32 */

int
threadspawn(int fd[3], char *cmd, char *argv[])
{
	return _runthreadspawn(fd, cmd, argv, nil);
}

int
threadspawnd(int fd[3], char *cmd, char *argv[], char *dir)
{
	return _runthreadspawn(fd, cmd, argv, dir);
}

int
threadspawnl(int fd[3], char *cmd, ...)
{
	char **argv, *s;
	int n, pid;
	va_list arg;

	va_start(arg, cmd);
	for(n=0; va_arg(arg, char*) != nil; n++)
		;
	n++;
	va_end(arg);

	argv = malloc(n*sizeof(argv[0]));
	if(argv == nil)
		return -1;

	va_start(arg, cmd);
	for(n=0; (s=va_arg(arg, char*)) != nil; n++)
		argv[n] = s;
	argv[n] = 0;
	va_end(arg);

	pid = threadspawn(fd, cmd, argv);
	free(argv);
	return pid;
}

int
_threadexec(Channel *cpid, int fd[3], char *cmd, char *argv[])
{
	int pid;

	pid = threadspawn(fd, cmd, argv);
	if(cpid){
		if(pid < 0)
			chansendul(cpid, ~0);
		else
			chansendul(cpid, pid);
	}
	return pid;
}

void
threadexec(Channel *cpid, int fd[3], char *cmd, char *argv[])
{
	if(_threadexec(cpid, fd, cmd, argv) >= 0)
		threadexits("threadexec");
}

void
threadexecl(Channel *cpid, int fd[3], char *cmd, ...)
{
	char **argv, *s;
	int n, pid;
	va_list arg;

	va_start(arg, cmd);
	for(n=0; va_arg(arg, char*) != nil; n++)
		;
	n++;
	va_end(arg);

	argv = malloc(n*sizeof(argv[0]));
	if(argv == nil){
		if(cpid)
			chansendul(cpid, ~0);
		return;
	}

	va_start(arg, cmd);
	for(n=0; (s=va_arg(arg, char*)) != nil; n++)
		argv[n] = s;
	argv[n] = 0;
	va_end(arg);

	pid = _threadexec(cpid, fd, cmd, argv);
	free(argv);

	if(pid >= 0)
		threadexits("threadexecl");
}
