#include <lib9.h>

/*
 * Function-pointer signature must match the actual function it's
 * assigned to (threadsysfatal in libthread). Declaring it as variadic
 * here while threadsysfatal takes (char*, va_list) is undefined on
 * ABIs where variadic and va_list-taking calls disagree (notably
 * macOS arm64). On Linux x86_64 the two ABIs happen to align, which
 * is why this latent bug went unnoticed in plan9port for years.
 */
void (*_sysfatal)(char*, va_list);

void
sysfatal(char *fmt, ...)
{
	char buf[256];
	va_list arg;

	va_start(arg, fmt);
	if(_sysfatal){
		(*_sysfatal)(fmt, arg);
		/* threadsysfatal does not return; if a future hook does, fall through */
	}
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);

	__fixargv0();
	fprint(2, "%s: %s\n", argv0 ? argv0 : "<prog>", buf);
	exits("fatal");
}
