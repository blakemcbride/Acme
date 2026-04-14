#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/stat.h>

#ifdef _WIN32

Dir*
dirstat(char *file)
{
	struct stat st;
	Dir *d;
	char *s;

	if(stat(file, &st) < 0)
		return nil;

	d = mallocz(sizeof(Dir)+strlen(file)+1+16, 1);
	if(d == nil)
		return nil;
	s = (char*)&d[1];
	strcpy(s, file);
	/* use basename */
	d->name = strrchr(s, '/');
	if(d->name)
		d->name++;
	else
		d->name = s;
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
dirstat(char *file)
{
	struct stat lst;
	struct stat st;
	int nstr;
	Dir *d;
	char *str;

	if(lstat(file, &lst) < 0)
		return nil;
	st = lst;
	if((lst.st_mode&S_IFMT) == S_IFLNK)
		stat(file, &st);

	nstr = _p9dir(&lst, &st, file, nil, nil, nil);
	d = mallocz(sizeof(Dir)+nstr, 1);
	if(d == nil)
		return nil;
	str = (char*)&d[1];
	_p9dir(&lst, &st, file, d, &str, str+nstr);
	return d;
}

#endif
