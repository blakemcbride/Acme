#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <utime.h>
#include <sys/stat.h>

#ifdef _WIN32

int
dirwstat(char *file, Dir *dir)
{
	int ret;
	struct utimbuf ub;

	ret = 0;
	if(~dir->mode != 0){
		if(chmod(file, dir->mode) < 0)
			ret = -1;
	}
	if(~dir->mtime != 0){
		ub.actime = dir->mtime;
		ub.modtime = dir->mtime;
		if(utime(file, &ub) < 0)
			ret = -1;
	}
	/* truncate not available on Windows; skip length change */
	return ret;
}

#else

int
dirwstat(char *file, Dir *dir)
{
	int ret;
	struct utimbuf ub;

	/* BUG handle more */
	ret = 0;
	if(~dir->mode != 0){
		if(chmod(file, dir->mode) < 0)
			ret = -1;
	}
	if(~dir->mtime != 0){
		ub.actime = dir->mtime;
		ub.modtime = dir->mtime;
		if(utime(file, &ub) < 0)
			ret = -1;
	}
	if(~dir->length != 0){
		if(truncate(file, dir->length) < 0)
			ret = -1;
	}
	return ret;
}

#endif
