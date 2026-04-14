#ifdef _WIN32

#include <u.h>
#include <libc.h>
#include <windows.h>

char*
getuser(void)
{
	static char user[256];
	DWORD n;

	if(user[0])
		return user;
	n = sizeof user;
	if(!GetUserNameA(user, &n))
		strecpy(user, user+sizeof user, "none");
	return user;
}

#else

#include <u.h>
#include <pwd.h>
#include <libc.h>

char*
getuser(void)
{
	static char user[64];
	struct passwd *pw;

	pw = getpwuid(getuid());
	if(pw == nil)
		return "none";
	strecpy(user, user+sizeof user, pw->pw_name);
	return user;
}

#endif
