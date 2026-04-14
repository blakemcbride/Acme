#include <u.h>
#include <libc.h>

#ifdef _WIN32

int
execl(char *prog, ...)
{
	USED(prog);
	werrstr("execl not supported on Windows");
	return -1;
}

#else

int
execl(char *prog, ...)
{
	int i;
	va_list arg;
	char **argv;

	va_start(arg, prog);
	for(i=0; va_arg(arg, char*) != nil; i++)
		;
	va_end(arg);

	argv = malloc((i+1)*sizeof(char*));
	if(argv == nil)
		return -1;

	va_start(arg, prog);
	for(i=0; (argv[i] = va_arg(arg, char*)) != nil; i++)
		;
	va_end(arg);

	exec(prog, argv);
	free(argv);
	return -1;
}

#endif
