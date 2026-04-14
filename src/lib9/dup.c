#include <u.h>
#include <libc.h>

#undef dup

#ifdef _WIN32

#include <io.h>

int
p9dup(int old, int new)
{
	if(new == -1)
		return _dup(old);
	return _dup2(old, new);
}

#else

int
p9dup(int old, int new)
{
	if(new == -1)
		return dup(old);
	return dup2(old, new);
}

#endif
