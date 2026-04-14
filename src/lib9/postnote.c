#ifdef _WIN32

#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

int
postnote(int who, int pid, char *msg)
{
	USED(who);
	USED(pid);
	USED(msg);
	werrstr("postnote not yet implemented on Windows");
	return -1;
}

#else

#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <signal.h>


extern int _p9strsig(char*);

int
postnote(int who, int pid, char *msg)
{
	int sig;

	sig = _p9strsig(msg);
	if(sig == 0){
		werrstr("unknown note");
		return -1;
	}

	if(pid <= 0){
		werrstr("bad pid in postnote");
		return -1;
	}

	switch(who){
	default:
		werrstr("bad who in postnote");
		return -1;
	case PNPROC:
		return kill(pid, sig);
	case PNGROUP:
		if((pid = getpgid(pid)) < 0)
			return -1;
		return killpg(pid, sig);
	}
}

#endif
