#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

void
p9longjmp(p9jmp_buf buf, int val)
{
#if defined(_WIN32)
	longjmp(*(jmp_buf*)buf, val);
#elif defined(__CYGWIN__) || defined(__MSYS__)
	_longjmp(*(jmp_buf*)buf, val);
#else
	siglongjmp(*(sigjmp_buf*)buf, val);
#endif
}

void
p9notejmp(void *x, p9jmp_buf buf, int val)
{
	USED(x);
#if defined(_WIN32)
	longjmp(*(jmp_buf*)buf, val);
#elif defined(__CYGWIN__) || defined(__MSYS__)
	_longjmp(*(jmp_buf*)buf, val);
#else
	siglongjmp(*(sigjmp_buf*)buf, val);
#endif
}
