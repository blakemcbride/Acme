#include <u.h>
#include <libc.h>

#ifdef _WIN32

int
exec(char *prog, char *argv[])
{
	USED(prog);
	USED(argv);
	werrstr("exec not supported on Windows");
	return -1;
}

#else

int
exec(char *prog, char *argv[])
{
	/* to mimic plan 9 should be just exec, but execvp is a better fit for unix */
	return execvp(prog, argv);
}

#endif
