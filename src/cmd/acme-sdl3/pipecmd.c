/*
 * Direct pipe command support for standalone acme.
 *
 * Replaces the 9P-based rdsel/wrsel/cons mechanism with direct
 * pipes and temp files.  This allows pipe commands (|sort, <cmd,
 * >cmd) to work without 9pserve or a 9P filesystem.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

/*
 * Queued pipe output for insertion into window body at wrselrange.
 */
typedef struct Pipeout Pipeout;
struct Pipeout
{
	int	winid;
	Rune	*r;
	int	nr;
	Pipeout	*next;
};

static QLock	pipeoutlk;
static Pipeout	*pipeouts;
static Pipeout	**pipeoutstail = &pipeouts;

static void
addpipeout(int winid, Rune *r, int nr)
{
	Pipeout *p;

	p = emalloc(sizeof(Pipeout));
	p->winid = winid;
	p->r = runemalloc(nr);
	runemove(p->r, r, nr);
	p->nr = nr;
	p->next = nil;
	qlock(&pipeoutlk);
	*pipeoutstail = p;
	pipeoutstail = &p->next;
	qunlock(&pipeoutlk);
	nbsendp(cptyout, nil);
}

/*
 * Called from mousethread with row.lk held.
 * Inserts queued pipe command output into windows at wrselrange.
 */
void
flushpipeout(void)
{
	Pipeout *p, *next, *list;
	Window *w;
	Text *t;
	uint q0;
	int nr;

	qlock(&pipeoutlk);
	list = pipeouts;
	pipeouts = nil;
	pipeoutstail = &pipeouts;
	qunlock(&pipeoutlk);

	for(p = list; p; p = next){
		next = p->next;
		w = lookid(p->winid, 0);
		if(w != nil){
			winlock(w, 'F');
			t = &w->body;
			wincommit(w, t);
			q0 = w->wrselrange.q1;
			if(q0 > t->file->b.nc)
				q0 = t->file->b.nc;
			q0 = textbsinsert(t, q0, p->r, p->nr, TRUE, &nr);
			w->wrselrange.q1 += nr;
			textsetselect(t, t->q0, t->q1);
			textshow(t, min(w->wrselrange.q0, t->file->b.nc),
				min(w->wrselrange.q1, t->file->b.nc), 1);
			textscrdraw(t);
			winsettag(w);
			w->nomark = FALSE;
			winunlock(w);
		}
		free(p->r);
		free(p);
	}
}

typedef struct Pipearg Pipearg;
struct Pipearg
{
	int	fd;
	int	winid;
};

/*
 * Reader proc: reads pipe output until EOF, then queues
 * for insertion into window at wrselrange.
 */
static void
pipereadproc(void *v)
{
	Pipearg *a;
	int fd, winid, n, nr, nulls;
	int bufsz, total;
	char *buf;
	Rune *rbuf;

	a = v;
	fd = a->fd;
	winid = a->winid;
	free(a);

	threadsetname("pipereadproc");

	bufsz = 8192;
	buf = emalloc(bufsz);
	total = 0;

	while((n = read(fd, buf + total, bufsz - total - 1)) > 0){
		total += n;
		if(total >= bufsz - 1){
			bufsz *= 2;
			buf = erealloc(buf, bufsz);
		}
	}
	close(fd);

	if(total > 0){
		buf[total] = 0;
		rbuf = runemalloc(total + 1);
		cvttorunes(buf, total, rbuf, &n, &nr, &nulls);
		if(nr > 0)
			addpipeout(winid, rbuf, nr);
		free(rbuf);
	}
	free(buf);
}

/*
 * Read the window's selection into a temp file.
 * If docut, also cut the selection and prepare wrselrange for output.
 * Returns the temp file fd seeked to 0, or -1 on failure.
 */
int
pipesel(int winid, int docut)
{
	Window *w;
	Text *t;
	int fd;
	uint q0, q1, n, m;
	Rune *r;
	char *s;

	w = lookid(winid, 0);
	if(w == nil)
		return -1;

	fd = tempfile();
	if(fd < 0)
		return -1;

	winlock(w, 'E');
	t = &w->body;
	q0 = t->q0;
	q1 = t->q1;

	r = fbufalloc();
	s = fbufalloc();
	while(q0 < q1){
		n = q1 - q0;
		if(n > BUFSIZE/UTFmax)
			n = BUFSIZE/UTFmax;
		bufread(&t->file->b, q0, r, n);
		m = snprint(s, BUFSIZE+1, "%.*S", n, r);
		if(write(fd, s, m) != m){
			warning(nil, "can't write temp file for pipe command %r\n");
			break;
		}
		q0 += n;
	}
	fbuffree(s);
	fbuffree(r);

	if(docut){
		seq++;
		filemark(t->file);
		cut(t, t, nil, FALSE, TRUE, nil, 0);
		w->wrselrange = range(t->q1, t->q1);
		w->nomark = TRUE;
	}

	winunlock(w);
	seek(fd, 0, 0);
	return fd;
}

/*
 * Set up a pipe for writing command output back to the window.
 * Spawns a reader proc that will insert output at wrselrange.
 * Returns the write end of the pipe (for the child's stdout).
 */
int
pipewrsel(int winid)
{
	int p[2];
	Pipearg *a;

	if(pipe(p) < 0)
		return open("/dev/null", OWRITE);

	a = emalloc(sizeof(Pipearg));
	a->fd = p[0];
	a->winid = winid;
	proccreate(pipereadproc, a, STACK);

	return p[1];
}
