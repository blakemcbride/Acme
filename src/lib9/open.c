#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#include <dirent.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/file.h>
#endif
#include <sys/stat.h>
#define NOPLAN9DEFINES
#include <libc.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct WinDir WinDir;
struct WinDir {
	HANDLE h;		/* from FindFirstFileA */
	WIN32_FIND_DATAA fd;
	int first;		/* 1 if fd from FindFirstFile not yet consumed */
	char path[MAX_PATH];
};

static struct {
	Lock lk;
	WinDir **d;
	int nd;
} wdirs;

static int
wdirput(int fd, WinDir *d)
{
	int i, nd;
	WinDir **dp;
	if(fd < 0)
		return -1;
	lock(&wdirs.lk);
	if(fd >= wdirs.nd){
		nd = wdirs.nd*2;
		if(nd <= fd) nd = fd+1;
		dp = realloc(wdirs.d, nd*sizeof wdirs.d[0]);
		if(dp == nil){ unlock(&wdirs.lk); return -1; }
		for(i=wdirs.nd; i<nd; i++) dp[i] = nil;
		wdirs.d = dp;
		wdirs.nd = nd;
	}
	wdirs.d[fd] = d;
	unlock(&wdirs.lk);
	return 0;
}

static WinDir*
wdirget(int fd)
{
	WinDir *d = nil;
	lock(&wdirs.lk);
	if(0 <= fd && fd < wdirs.nd)
		d = wdirs.d[fd];
	unlock(&wdirs.lk);
	return d;
}

static WinDir*
wdirdel(int fd)
{
	WinDir *d = nil;
	lock(&wdirs.lk);
	if(0 <= fd && fd < wdirs.nd){
		d = wdirs.d[fd];
		wdirs.d[fd] = nil;
	}
	unlock(&wdirs.lk);
	return d;
}
#endif

static struct {
	Lock lk;
	DIR **d;
	int nd;
} dirs;

static int
dirput(int fd, DIR *d)
{
	int i, nd;
	DIR **dp;

	if(fd < 0) {
		werrstr("invalid fd");
		return -1;
	}
	lock(&dirs.lk);
	if(fd >= dirs.nd) {
		nd = dirs.nd*2;
		if(nd <= fd)
			nd = fd+1;
		dp = realloc(dirs.d, nd*sizeof dirs.d[0]);
		if(dp == nil) {
			werrstr("out of memory");
			unlock(&dirs.lk);
			return -1;
		}
		for(i=dirs.nd; i<nd; i++)
			dp[i] = nil;
		dirs.d = dp;
		dirs.nd = nd;
	}
	dirs.d[fd] = d;
	unlock(&dirs.lk);
	return 0;
}

static DIR*
dirget(int fd)
{
	DIR *d;

	lock(&dirs.lk);
	d = nil;
	if(0 <= fd && fd < dirs.nd)
		d = dirs.d[fd];
	unlock(&dirs.lk);
	return d;
}

static DIR*
dirdel(int fd)
{
	DIR *d;

	lock(&dirs.lk);
	d = nil;
	if(0 <= fd && fd < dirs.nd) {
		d = dirs.d[fd];
		dirs.d[fd] = nil;
	}
	unlock(&dirs.lk);
	return d;
}

int
p9create(char *path, int mode, ulong perm)
{
	int fd, cexec, umode, rclose, lock, rdwr;
#ifndef _WIN32
	struct flock fl;
#endif

	rdwr = mode&3;
	lock = mode&OLOCK;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	mode &= ~(ORCLOSE|OCEXEC|OLOCK);

	/* XXX should get mode mask right? */
	fd = -1;
	if(perm&DMDIR){
		if(mode != OREAD){
			werrstr("bad mode in directory create");
			goto out;
		}
#ifdef _WIN32
		mkdir(path);
#else
		if(mkdir(path, perm&0777) < 0)
			goto out;
#endif
		fd = open(path, O_RDONLY);
	}else{
		umode = (mode&3)|O_CREAT|O_TRUNC;
		mode &= ~(3|OTRUNC);
#ifdef O_DIRECT
		if(mode&ODIRECT){
			umode |= O_DIRECT;
			mode &= ~ODIRECT;
		}
#endif
		if(mode&OEXCL){
			umode |= O_EXCL;
			mode &= ~OEXCL;
		}
		if(mode&OAPPEND){
			umode |= O_APPEND;
			mode &= ~OAPPEND;
		}
		if(mode){
			werrstr("unsupported mode in create");
			goto out;
		}
		fd = open(path, umode, perm);
	}
out:
	if(fd >= 0){
#ifndef _WIN32
		if(lock){
			fl.l_type = (rdwr==OREAD) ? F_RDLCK : F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			if(fcntl(fd, F_SETLK, &fl) < 0){
				close(fd);
				werrstr("lock: %r");
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
#else
		USED(lock);
		USED(rdwr);
		USED(cexec);
#endif
		if(rclose)
			remove(path);
	}
	return fd;
}

int
p9open(char *name, int mode)
{
	int cexec, rclose;
	int fd, umode, lock, rdwr;
#ifndef _WIN32
	struct flock fl;
#endif
	struct stat st;
	DIR *d;

	rdwr = mode&3;
	umode = rdwr;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	lock = mode&OLOCK;
	mode &= ~(3|OCEXEC|ORCLOSE|OLOCK);
	if(mode&OTRUNC){
		umode |= O_TRUNC;
		mode ^= OTRUNC;
	}
#ifdef O_DIRECT
	if(mode&ODIRECT){
		umode |= O_DIRECT;
		mode ^= ODIRECT;
	}
#else
	if(mode&ODIRECT)
		mode ^= ODIRECT;
#endif
#ifdef O_NONBLOCK
	if(mode&ONONBLOCK){
		umode |= O_NONBLOCK;
		mode ^= ONONBLOCK;
	}
#else
	if(mode&ONONBLOCK)
		mode ^= ONONBLOCK;
#endif
	if(mode&OAPPEND){
		umode |= O_APPEND;
		mode ^= OAPPEND;
	}
	if(mode){
		werrstr("mode 0x%x not supported", mode);
		return -1;
	}
#ifdef _WIN32
	/* On Windows, open() can't open directories.  Detect and handle. */
	if(stat(name, &st) >= 0 && (st.st_mode & _S_IFDIR)){
		HANDLE h = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, nil,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nil);
		if(h == INVALID_HANDLE_VALUE){
			werrstr("can't open directory");
			return -1;
		}
		fd = _open_osfhandle((intptr_t)h, _O_RDONLY);
		if(fd < 0){
			CloseHandle(h);
			return -1;
		}
		WinDir *wd = mallocz(sizeof(WinDir), 1);
		if(wd == nil){
			close(fd);
			return -1;
		}
		wd->h = INVALID_HANDLE_VALUE;
		wd->first = 0;
		strncpy(wd->path, name, sizeof wd->path - 1);
		if(wdirput(fd, wd) < 0){
			free(wd);
			close(fd);
			return -1;
		}
		return fd;
	}
#endif
	fd = open(name, umode);
	if(fd >= 0){
#ifndef _WIN32
		if(lock){
			fl.l_type = (rdwr==OREAD) ? F_RDLCK : F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			if(fcntl(fd, F_SETLK, &fl) < 0){
				close(fd);
				werrstr("lock: %r");
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(fstat(fd, &st) >= 0 && S_ISDIR(st.st_mode)) {
			d = fdopendir(fd);
			if(d == nil) {
				close(fd);
				return -1;
			}
			if(dirput(fd, d) < 0) {
				closedir(d);
				return -1;
			}
		}
#else
		USED(lock);
		USED(rdwr);
		USED(cexec);
		USED(st);
		USED(d);
		/* On Windows, directory reading is handled differently */
#endif
		if(rclose)
			remove(name);
	}
	return fd;
}

vlong
p9seek(int fd, vlong offset, int whence)
{
	DIR *d;

	if((d = dirget(fd)) != nil) {
		if(whence == 1 && offset == 0)
			return telldir(d);
		if(whence == 0) {
			seekdir(d, offset);
			return 0;
		}
		werrstr("bad seek in directory");
		return -1;
	}

	return lseek(fd, offset, whence);
}

int
p9close(int fd)
{
#ifdef _WIN32
	WinDir *wd;
	if((wd = wdirdel(fd)) != nil){
		if(wd->h != INVALID_HANDLE_VALUE)
			FindClose(wd->h);
		free(wd);
		return close(fd);
	}
	return close(fd);
#else
	DIR *d;
	if((d = dirdel(fd)) != nil)
		return closedir(d);
	return close(fd);
#endif
}

typedef struct DirBuild DirBuild;
struct DirBuild {
	Dir *d;
	int nd;
	int md;
	char *str;
	char *estr;
};

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

#ifndef _WIN32

static int
dirbuild1(DirBuild *b, struct stat *lst, struct stat *st, char *name)
{
	int i, nstr;
	Dir *d;
	int md, mstr;
	char *lo, *hi, *newlo;

	nstr = _p9dir(lst, st, name, nil, nil, nil);
	if(b->md-b->nd < 1 || b->estr-b->str < nstr) {
		// expand either d space or str space or both.
		md = b->md;
		if(b->md-b->nd < 1) {
			md *= 2;
			if(md < 16)
				md = 16;
		}
		mstr = b->estr-(char*)&b->d[b->md];
		if(b->estr-b->str < nstr) {
			mstr += nstr;
			mstr += mstr/2;
		}
		if(mstr < 512)
			mstr = 512;
		d = realloc(b->d, md*sizeof d[0] + mstr);
		if(d == nil)
			return -1;
		// move strings and update pointers in Dirs
		lo = (char*)&b->d[b->md];
		newlo = (char*)&d[md];
		hi = b->str;
		memmove(newlo, lo+((char*)d-(char*)b->d), hi-lo);
		for(i=0; i<b->nd; i++) {
			if(lo <= d[i].name && d[i].name < hi)
				d[i].name += newlo - lo;
			if(lo <= d[i].uid && d[i].uid < hi)
				d[i].uid += newlo - lo;
			if(lo <= d[i].gid && d[i].gid < hi)
				d[i].gid += newlo - lo;
			if(lo <= d[i].muid && d[i].muid < hi)
				d[i].muid += newlo - lo;
		}
		b->d = d;
		b->md = md;
		b->str += newlo - lo;
		b->estr = newlo + mstr;
	}
	_p9dir(lst, st, name, &b->d[b->nd], &b->str, b->estr);
	b->nd++;
	return 0;
}

static long
dirreadmax(int fd, Dir **dp, int max)
{
	int i;
	DIR *dir;
	DirBuild b;
	struct dirent *de;
	struct stat st, lst;

	if((dir = dirget(fd)) == nil) {
		werrstr("not a directory");
		return -1;
	}

	memset(&b, 0, sizeof b);
	for(i=0; max == -1 || i<max; i++) { // max = not too many, not too few
		errno = 0;
		de = readdir(dir);
		if(de == nil) {
			if(b.nd == 0 && errno != 0)
				return -1;
			break;
		}
		// Note: not all systems have d_namlen. Assume NUL-terminated.
		if(de->d_name[0]=='.' && de->d_name[1]==0)
			continue;
		if(de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]==0)
			continue;
		if(fstatat(fd, de->d_name, &lst, AT_SYMLINK_NOFOLLOW) < 0)
			continue;
		st = lst;
		if(S_ISLNK(lst.st_mode))
			fstatat(fd, de->d_name, &st, 0);
		dirbuild1(&b, &lst, &st, de->d_name);
	}
	*dp = b.d;
	return b.nd;
}

long
dirread(int fd, Dir **dp)
{
	return dirreadmax(fd, dp, 10);
}

long
dirreadall(int fd, Dir **dp)
{
	return dirreadmax(fd, dp, -1);
}

#else /* _WIN32 */

static Dir*
winfd_to_dir(WIN32_FIND_DATAA *fd)
{
	Dir *d;
	char *name;
	int namelen;
	ULARGE_INTEGER size, mtime, atime;

	namelen = strlen(fd->cFileName);
	d = mallocz(sizeof(Dir) + namelen + 1 + 4 + 4 + 1, 1);
	if(d == nil)
		return nil;
	name = (char*)&d[1];
	memmove(name, fd->cFileName, namelen+1);
	d->name = name;
	d->uid = name + namelen + 1;
	strcpy(d->uid, "none");
	d->gid = d->uid;
	d->muid = "";
	size.HighPart = fd->nFileSizeHigh;
	size.LowPart = fd->nFileSizeLow;
	d->length = size.QuadPart;
	d->mode = 0644;
	/* Windows FILETIME is 100ns intervals since 1601; convert to Unix epoch */
	mtime.HighPart = fd->ftLastWriteTime.dwHighDateTime;
	mtime.LowPart = fd->ftLastWriteTime.dwLowDateTime;
	d->mtime = (mtime.QuadPart - 116444736000000000ULL) / 10000000ULL;
	atime.HighPart = fd->ftLastAccessTime.dwHighDateTime;
	atime.LowPart = fd->ftLastAccessTime.dwLowDateTime;
	d->atime = (atime.QuadPart - 116444736000000000ULL) / 10000000ULL;
	d->qid.path = 0;
	d->qid.vers = d->mtime;
	d->qid.type = 0;
	if(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
		d->mode = DMDIR | 0755;
		d->qid.type = QTDIR;
		d->length = 0;
	}
	d->type = 'M';
	return d;
}

static long
winreadmax(int fd, Dir **dp, int max)
{
	WinDir *wd;
	Dir *arr, *tmp;
	char pattern[MAX_PATH + 4];
	int n, cap;

	wd = wdirget(fd);
	if(wd == nil){
		werrstr("not a directory");
		return -1;
	}

	if(wd->h == INVALID_HANDLE_VALUE){
		/* First call: start the enumeration */
		snprint(pattern, sizeof pattern, "%s/*", wd->path);
		wd->h = FindFirstFileA(pattern, &wd->fd);
		if(wd->h == INVALID_HANDLE_VALUE){
			*dp = nil;
			return 0;
		}
		wd->first = 1;
	}

	arr = nil;
	n = 0;
	cap = 0;
	for(;;){
		if(max >= 0 && n >= max)
			break;
		if(wd->first){
			wd->first = 0;
		}else{
			if(!FindNextFileA(wd->h, &wd->fd))
				break;
		}
		/* skip "." and ".." */
		if(wd->fd.cFileName[0]=='.' && wd->fd.cFileName[1]==0)
			continue;
		if(wd->fd.cFileName[0]=='.' && wd->fd.cFileName[1]=='.' && wd->fd.cFileName[2]==0)
			continue;
		if(n >= cap){
			cap = cap ? cap*2 : 16;
			tmp = realloc(arr, cap * sizeof(Dir));
			if(tmp == nil){
				free(arr);
				return -1;
			}
			arr = tmp;
		}
		{
			Dir *entry = winfd_to_dir(&wd->fd);
			if(entry == nil) continue;
			arr[n] = *entry;
			/* fix up string pointers - they point inside entry */
			arr[n].name = strdup(entry->name);
			arr[n].uid = strdup(entry->uid);
			arr[n].gid = arr[n].uid;
			arr[n].muid = "";
			free(entry);
			n++;
		}
	}
	*dp = arr;
	return n;
}

long
dirread(int fd, Dir **dp)
{
	return winreadmax(fd, dp, 10);
}

long
dirreadall(int fd, Dir **dp)
{
	return winreadmax(fd, dp, -1);
}

#endif
