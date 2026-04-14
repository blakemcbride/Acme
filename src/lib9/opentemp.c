#include <u.h>
#include <libc.h>

#ifdef _WIN32

#include <io.h>
#include <fcntl.h>

int
opentemp(char *template, int mode)
{
	int fd;

	if(_mktemp(template) == nil)
		return -1;
	fd = _open(template, mode|O_CREAT|O_EXCL|_O_BINARY, 0600);
	return fd;
}

#else

int
opentemp(char *template, int mode)
{
	int fd, fd1;

	fd = mkstemp(template);
	if(fd < 0)
		return -1;
	if((fd1 = open(template, mode)) < 0){
		remove(template);
		close(fd);
		return -1;
	}
	close(fd);
	return fd1;
}

#endif
