#include <u.h>
#include <libc.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char*
get9root(void)
{
	static char *s;
	char buf[MAX_PATH];
	char *p;

	if(s)
		return s;

	if((s = getenv("PLAN9")) != 0)
		return s;

	/* Derive from executable path: .../bin/acme.exe → ... */
	if(GetModuleFileNameA(nil, buf, sizeof buf) > 0){
		/* convert backslashes to forward slashes */
		for(p = buf; *p; p++)
			if(*p == '\\')
				*p = '/';
		/* strip /bin/acme.exe (or similar) */
		p = strrchr(buf, '/');
		if(p){
			*p = '\0';	/* strip /acme.exe */
			p = strrchr(buf, '/');
			if(p)
				*p = '\0';	/* strip /bin */
		}
		s = strdup(buf);
		return s;
	}

	s = PLAN9_TARGET;
	return s;
}

#else

char*
get9root(void)
{
	static char *s;

	if(s)
		return s;

	if((s = getenv("PLAN9")) != 0)
		return s;
	s = PLAN9_TARGET;
	return s;
}

#endif
