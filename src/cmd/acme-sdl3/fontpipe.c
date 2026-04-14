/*
 * Embedded fontsrv: replaces fork/exec of fontsrv with in-process
 * FreeType/fontconfig font rendering.
 *
 * Provides _fontpipe() that generates font data via a pipe,
 * using fontsrv's x11.c rendering engine linked directly.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <memdraw.h>
#include "../fontsrv/a.h"

/* These are declared extern in a.h but defined in fontsrv/main.c
 * which we don't compile. Define them here. */
XFont *xfont;
int nxfont;

static int fontsrv_ready;
static QLock fontsrv_lk;

int fontcmp(const void*, const void*);

static void
fontsrv_init(void)
{
	qlock(&fontsrv_lk);
	if(!fontsrv_ready){
		loadfonts();
		qsort(xfont, nxfont, sizeof xfont[0], fontcmp);
		fontsrv_ready = 1;
	}
	qunlock(&fontsrv_lk);
}

int
fontcmp(const void *va, const void *vb)
{
	XFont *a, *b;

	a = (XFont*)va;
	b = (XFont*)vb;
	return strcmp(a->name, b->name);
}

void*
emalloc9p(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal("emalloc9p: out of memory");
	memset(p, 0, n);
	return p;
}

static void
packinfo(Fontchar *fc, uchar *p, int n)
{
	int j;

	for(j=0; j<=n; j++){
		p[0] = fc->x;
		p[1] = fc->x>>8;
		p[2] = fc->top;
		p[3] = fc->bottom;
		p[4] = fc->left;
		p[5] = fc->width;
		fc++;
		p += 6;
	}
}

typedef struct Fontarg Fontarg;
struct Fontarg
{
	int	fd;		/* write end of pipe */
	int	fontidx;
	int	size;
	int	antialias;
	int	issubfont;	/* 0 = font header, 1 = subfont bitmap */
	int	range;		/* subfont range index */
};

static void
writefontfile(Fontarg *a)
{
	XFont *f;
	int height, ascent, i;
	Fmt fmt;
	char *s;

	f = &xfont[a->fontidx];
	qlock(&fontsrv_lk);
	load(f);
	qunlock(&fontsrv_lk);

	if(f->unit == 0 && f->loadheight == nil){
		fprint(a->fd, "font missing\n");
		return;
	}

	height = 0;
	ascent = 0;
	if(f->unit > 0){
		height = f->height * a->size/f->unit + 0.99999999;
		ascent = height - (int)(-f->originy * a->size/f->unit + 0.99999999);
	}
	if(f->loadheight != nil)
		f->loadheight(f, a->size, &height, &ascent);

	fmtstrinit(&fmt);
	fmtprint(&fmt, "%11d %11d\n", height, ascent);
	for(i=0; i<f->nfile; i++)
		fmtprint(&fmt, "0x%06x 0x%06x x%06x.bit\n",
			f->file[i]*SubfontSize,
			((f->file[i]+1)*SubfontSize) - 1,
			f->file[i]*SubfontSize);
	s = fmtstrflush(&fmt);
	write(a->fd, s, strlen(s));
	free(s);
}

static void
writesubfontfile(Fontarg *a)
{
	XFont *f;
	Memsubfont *sf;
	Memimage *m;
	int size;
	uchar *info;
	char hdr[5*12+1], shdr[3*12+1];

	f = &xfont[a->fontidx];
	qlock(&fontsrv_lk);
	load(f);
	sf = mksubfont(f, f->name, a->range*SubfontSize,
		((a->range+1)*SubfontSize)-1, a->size, a->antialias);
	qunlock(&fontsrv_lk);

	if(sf == nil)
		return;

	m = sf->bits;

	/* write image header */
	snprint(hdr, sizeof hdr, "%11s %11d %11d %11d %11d ",
		a->antialias ? "k8" : "k1",
		m->r.min.x, m->r.min.y, m->r.max.x, m->r.max.y);
	write(a->fd, hdr, 5*12);

	/* write raw pixel data */
	size = bytesperline(m->r, chantodepth(m->chan)) * Dy(m->r);
	write(a->fd, byteaddr(m, m->r.min), size);

	/* write subfont info */
	snprint(shdr, sizeof shdr, "%11d %11d %11d ", sf->n, sf->height, sf->ascent);
	write(a->fd, shdr, 3*12);

	info = malloc(6*(sf->n+1));
	if(info != nil){
		packinfo(sf->info, info, sf->n);
		write(a->fd, info, 6*(sf->n+1));
		free(info);
	}
	freememimage(sf->bits);
	free(sf->info);
	free(sf);
}

static void
fontproc(void *v)
{
	Fontarg *a;

	a = v;
	if(a->issubfont)
		writesubfontfile(a);
	else
		writefontfile(a);
	close(a->fd);
	free(a);
}

/*
 * Parse a font path like "DejaVuSans/13a/font" or "DejaVuSans/13a/x000000.bit"
 * Returns -1 on error, fills in Fontarg fields.
 */
static int
parsefontpath(char *name, Fontarg *a)
{
	char *buf, *fontname, *sizep, *filep, *p;
	int i, size, antialias;

	buf = strdup(name);
	if(buf == nil)
		return -1;

	/* split into fontname/size/file */
	fontname = buf;
	sizep = strchr(fontname, '/');
	if(sizep == nil){
		free(buf);
		return -1;
	}
	*sizep++ = '\0';

	filep = strchr(sizep, '/');
	if(filep == nil){
		free(buf);
		return -1;
	}
	*filep++ = '\0';

	/* parse size and antialias flag */
	size = strtol(sizep, &p, 10);
	if(size <= 0){
		free(buf);
		return -1;
	}
	antialias = 0;
	if(*p == 'a'){
		antialias = 1;
		p++;
	}
	if(*p != '\0'){
		free(buf);
		return -1;
	}

	/* find font by name */
	a->fontidx = -1;
	for(i=0; i<nxfont; i++){
		if(strcmp(xfont[i].name, fontname) == 0){
			a->fontidx = i;
			break;
		}
	}
	if(a->fontidx < 0){
		free(buf);
		return -1;
	}

	a->size = size;
	a->antialias = antialias;

	if(strcmp(filep, "font") == 0){
		a->issubfont = 0;
		a->range = 0;
	}else if(filep[0] == 'x' && strlen(filep) > 7 && strcmp(filep+7, ".bit") == 0){
		a->range = strtoul(filep+1, &p, 16);
		if(p != filep+7 || a->range % SubfontSize != 0){
			free(buf);
			return -1;
		}
		a->range /= SubfontSize;
		a->issubfont = 1;
	}else{
		free(buf);
		return -1;
	}

	free(buf);
	return 0;
}

int
_fontpipe(char *name)
{
	int p[2];
	Fontarg *a;

	/* strip /mnt/font/ prefix if present (backward compat) */
	if(strncmp(name, "/mnt/font/", 10) == 0)
		name += 10;

	if(!fontsrv_ready)
		fontsrv_init();

	a = malloc(sizeof(Fontarg));
	if(a == nil)
		return -1;

	if(parsefontpath(name, a) < 0){
		free(a);
		werrstr("bad font path: %s", name);
		return -1;
	}

	if(pipe(p) < 0){
		free(a);
		return -1;
	}

	a->fd = p[1];
	proccreate(fontproc, a, 256*1024);
	return p[0];
}
