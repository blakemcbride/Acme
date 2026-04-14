#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "fsimpl.h"

int
fsopenfd(CFsys *fs, char *name, int mode)
{
	CFid *fid;
	Fcall tx, rx;

	if((fid = fswalk(fs->root, name)) == nil)
		return -1;
	tx.type = Topenfd;
	tx.fid = fid->fid;
	tx.mode = mode&~OCEXEC;
	if(_fsrpc(fs, &tx, &rx, 0) < 0){
		fsclose(fid);
		return -1;
	}
	_fsputfid(fid);
	if(mode&OCEXEC && rx.unixfd>=0){
#if !defined(_WIN32) || defined(__CYGWIN__)
		fcntl(rx.unixfd, F_SETFL, FD_CLOEXEC);
#endif
	}
	return rx.unixfd;
}
