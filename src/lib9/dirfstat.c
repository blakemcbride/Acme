#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/stat.h>

#ifdef _WIN32

Dir*
dirfstat(int fd)
{
	struct stat st;
	Dir *d;

	if(fstat(fd, &st) < 0)
		return nil;

	d = mallocz(sizeof(Dir)+128, 1);
	if(d == nil)
		return nil;
	d->name = "unknown";
	d->uid = "none";
	d->gid = "none";
	d->muid = "";
	d->atime = st.st_atime;
	d->mtime = st.st_mtime;
	d->length = st.st_size;
	d->mode = st.st_mode & 0777;
	d->qid.path = st.st_ino;
	d->qid.vers = st.st_mtime;
	if(st.st_mode & _S_IFDIR){
		d->mode |= DMDIR;
		d->qid.type = QTDIR;
		d->length = 0;
	}
	d->type = 'M';
	return d;
}

#else

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

Dir*
dirfstat(int fd)
{
	struct stat st;
	int nstr;
	Dir *d;
	char *str, tmp[100];

	if(fstat(fd, &st) < 0)
		return nil;

	snprint(tmp, sizeof tmp, "/dev/fd/%d", fd);
	nstr = _p9dir(&st, &st, tmp, nil, nil, nil);
	d = mallocz(sizeof(Dir)+nstr, 1);
	if(d == nil)
		return nil;
	str = (char*)&d[1];
	_p9dir(&st, &st, tmp, d, &str, str+nstr);
	return d;
}

#endif
