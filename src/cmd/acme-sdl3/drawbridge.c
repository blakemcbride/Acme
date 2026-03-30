/*
 * Direct-linkage bridge: replaces drawclient.c
 *
 * Instead of fork/exec devdraw and communicating over a pipe,
 * this module links devdraw's SDL3 backend directly into the process
 * and calls draw_datawrite/draw_dataread in-process.
 *
 * Events are delivered via Rendez condition variables instead of
 * pipe-based RPC tag matching.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "../devdraw/devdraw.h"

extern Mouse _drawmouse;
int chattydrawclient = 0;

/* SDL3 readiness synchronization */
int bridge_sdl3ready;
QLock bridge_sdl3readylk;
Rendez bridge_sdl3readywait;

/* Event delivery: reader blocks on these when no events available */
Rendez bridge_mouserdz;
Rendez bridge_kbdrdz;

static void
sdl3proc(void *v)
{
	USED(v);
	gfx_main();
}

int
_displayconnect(Display *d)
{
	fmtinstall('W', drawfcallfmt);
	fmtinstall('H', encodefmt);

	memimageinit();

	client0 = mallocz(sizeof(Client), 1);
	if(client0 == nil){
		werrstr("out of memory");
		return -1;
	}
	client0->displaydpi = 100;
	client0->rfd = -1;
	client0->wfd = -1;

	/* Set up event synchronization */
	bridge_mouserdz.l = &client0->eventlk;
	bridge_kbdrdz.l = &client0->eventlk;
	bridge_sdl3readywait.l = &bridge_sdl3readylk;

	/* Spawn SDL3 event loop in a new proc */
	proccreate(sdl3proc, nil, 256*1024);

	/* Wait for SDL3 initialization to complete */
	qlock(&bridge_sdl3readylk);
	while(!bridge_sdl3ready)
		rsleep(&bridge_sdl3readywait);
	qunlock(&bridge_sdl3readylk);

	d->srvfd = -1;
	return 0;
}

int
_displaymux(Display *d)
{
	USED(d);
	return 0;	/* no multiplexer needed in direct mode */
}

int
_displayinit(Display *d, char *label, char *winsize)
{
	Memimage *m;

	USED(d);

	m = rpc_attach(client0, label, winsize);
	if(m == nil)
		return -1;
	draw_initdisplaymemimage(client0, m);
	return 0;
}

int
_displayrdmouse(Display *d, Mouse *m, int *resized)
{
	USED(d);

	qlock(&client0->eventlk);
	client0->mouse.stall = 0;
	while(client0->mouse.ri == client0->mouse.wi)
		rsleep(&bridge_mouserdz);
	*m = client0->mouse.m[client0->mouse.ri++];
	if(client0->mouse.ri == nelem(client0->mouse.m))
		client0->mouse.ri = 0;
	*resized = client0->mouse.resized;
	client0->mouse.resized = 0;
	_drawmouse = *m;
	qunlock(&client0->eventlk);
	return 0;
}

int
_displayrdkbd(Display *d, Rune *r)
{
	USED(d);

	qlock(&client0->eventlk);
	client0->kbd.stall = 0;
	while(client0->kbd.ri == client0->kbd.wi)
		rsleep(&bridge_kbdrdz);
	*r = client0->kbd.r[client0->kbd.ri++];
	if(client0->kbd.ri == nelem(client0->kbd.r))
		client0->kbd.ri = 0;
	qunlock(&client0->eventlk);
	return 0;
}

int
_displaymoveto(Display *d, Point p)
{
	client0->impl->rpc_setmouse(client0, p);
	_drawmouse.xy = p;
	return flushimage(d, 1);
}

int
_displaycursor(Display *d, Cursor *c, Cursor2 *c2)
{
	Cursor2 c2buf;

	USED(d);

	if(c == nil)
		client0->impl->rpc_setcursor(client0, nil, nil);
	else{
		if(c2 == nil){
			scalecursor(&c2buf, c);
			c2 = &c2buf;
		}
		client0->impl->rpc_setcursor(client0, c, c2);
	}
	return 0;
}

int
_displaybouncemouse(Display *d, Mouse *m)
{
	USED(d);
	client0->impl->rpc_bouncemouse(client0, *m);
	return 0;
}

int
_displaylabel(Display *d, char *label)
{
	USED(d);
	client0->impl->rpc_setlabel(client0, label);
	return 0;
}

char*
_displayrdsnarf(Display *d)
{
	USED(d);
	return rpc_getsnarf();
}

int
_displaywrsnarf(Display *d, char *snarf)
{
	USED(d);
	rpc_putsnarf(snarf);
	return 0;
}

int
_displayrddraw(Display *d, void *v, int n)
{
	USED(d);
	return draw_dataread(client0, v, n);
}

int
_displaywrdraw(Display *d, void *v, int n)
{
	USED(d);
	if(draw_datawrite(client0, v, n) != n)
		return -1;
	return n;
}

int
_displaytop(Display *d)
{
	USED(d);
	client0->impl->rpc_topwin(client0);
	return 0;
}

int
_displayresize(Display *d, Rectangle r)
{
	USED(d);
	client0->impl->rpc_resizewindow(client0, r);
	return 0;
}
