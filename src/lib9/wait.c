#include <u.h>
#include <libc.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Defined in libthread/exec.c; returns and removes the HANDLE for pid. */
extern HANDLE _threadprochandle(int pid);

Waitmsg*
wait(void)
{
	werrstr("wait not yet implemented on Windows");
	return nil;
}

Waitmsg*
waitnohang(void)
{
	return nil;
}

Waitmsg*
waitfor(int pid)
{
	HANDLE h;
	DWORD code;
	Waitmsg *w;
	char buf[64];
	int l;

	h = _threadprochandle(pid);
	if(h == INVALID_HANDLE_VALUE){
		werrstr("waitfor: no handle for pid %d", pid);
		return nil;
	}
	if(WaitForSingleObject(h, INFINITE) == WAIT_FAILED){
		CloseHandle(h);
		werrstr("waitfor: WaitForSingleObject failed");
		return nil;
	}
	if(!GetExitCodeProcess(h, &code))
		code = (DWORD)-1;
	CloseHandle(h);

	if(code == 0)
		buf[0] = '\0';
	else
		snprint(buf, sizeof buf, "%lu", (unsigned long)code);
	l = strlen(buf) + 1;
	w = malloc(sizeof(Waitmsg) + l);
	if(w == nil)
		return nil;
	w->pid = pid;
	w->time[0] = 0;
	w->time[1] = 0;
	w->time[2] = 0;
	w->msg = (char*)&w[1];
	memmove(w->msg, buf, l);
	return w;
}

#else

static Waitmsg*
_wait(int n, char *buf)
{
	int l;
	char *fld[5];
	Waitmsg *w;

	if(n <= 0)
		return nil;
	buf[n] = '\0';
	if(tokenize(buf, fld, nelem(fld)) != nelem(fld)){
		werrstr("couldn't parse wait message");
		return nil;
	}
	l = strlen(fld[4])+1;
	w = malloc(sizeof(Waitmsg)+l);
	if(w == nil)
		return nil;
	w->pid = atoi(fld[0]);
	w->time[0] = atoi(fld[1]);
	w->time[1] = atoi(fld[2]);
	w->time[2] = atoi(fld[3]);
	w->msg = (char*)&w[1];
	memmove(w->msg, fld[4], l);
	return w;
}

Waitmsg*
wait(void)
{
	char buf[256];

	return _wait(await(buf, sizeof buf-1), buf);
}

Waitmsg*
waitnohang(void)
{
	char buf[256];

	return _wait(awaitnohang(buf, sizeof buf-1), buf);
}

Waitmsg*
waitfor(int pid)
{
	char buf[256];

	return _wait(awaitfor(pid, buf, sizeof buf-1), buf);
}

#endif
