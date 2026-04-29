/*
 * Direct-mode event dispatch for standalone acme.
 * Derived from srv.c: removes pipe-based RPC dispatch,
 * uses Rendez condition variables for event delivery.
 *
 * Kept: gfx_keystroke, gfx_mousetrack, gfx_abortcompose,
 *       gfx_mouseresized, gfx_started, kputc, client0
 * Removed: serveproc, runmsg, replymsg, matchkbd, matchmouse,
 *          listenproc, threadmain
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <memdraw.h>
#include <memlayer.h>
#include <keyboard.h>
#include <mouse.h>
#include <cursor.h>
#include <drawfcall.h>
#include "../devdraw/devdraw.h"

Client *client0;
int trace = 0;

/* Defined in drawbridge.c */
extern int bridge_sdl3ready;
extern QLock bridge_sdl3readylk;
extern Rendez bridge_sdl3readywait;
extern Rendez bridge_mouserdz;
extern Rendez bridge_kbdrdz;

static void	kputc(Client*, int);

/*
 * Signal that SDL3 has initialized.
 * Called from sdl3-screen.c gfx_main after SDL_Init succeeds.
 *
 * Since gfx_main runs on the OS main thread and may reach this
 * function before _displayconnect runs on the worker proc, set the
 * Rendez lock pointer here too (idempotent pointer assignment).
 */
void
gfx_started(void)
{
	bridge_sdl3readywait.l = &bridge_sdl3readylk;
	qlock(&bridge_sdl3readylk);
	bridge_sdl3ready = 1;
	rwakeup(&bridge_sdl3readywait);
	qunlock(&bridge_sdl3readylk);
}

void
gfx_mouseresized(Client *c)
{
	gfx_mousetrack(c, -1, -1, -1, -1);
}

void
gfx_mousetrack(Client *c, int x, int y, int b, uint ms)
{
	Mouse *m;

	qlock(&c->eventlk);
	if(x == -1 && y == -1 && b == -1 && ms == -1){
		Mouse *copy;
		/* repeat last mouse event for resize */
		if(c->mouse.ri == 0)
			copy = &c->mouse.m[nelem(c->mouse.m)-1];
		else
			copy = &c->mouse.m[c->mouse.ri-1];
		x = copy->xy.x;
		y = copy->xy.y;
		b = copy->buttons;
		ms = copy->msec;
		c->mouse.resized = 1;
	}
	if(x < c->mouserect.min.x)
		x = c->mouserect.min.x;
	if(x > c->mouserect.max.x)
		x = c->mouserect.max.x;
	if(y < c->mouserect.min.y)
		y = c->mouserect.min.y;
	if(y > c->mouserect.max.y)
		y = c->mouserect.max.y;

	/*
	 * If reader has stopped reading, don't bother.
	 * If reader is completely caught up, definitely queue.
	 * Otherwise, queue only button change events.
	 */
	if(!c->mouse.stall)
	if(c->mouse.wi == c->mouse.ri || c->mouse.last.buttons != b){
		m = &c->mouse.last;
		m->xy.x = x;
		m->xy.y = y;
		m->buttons = b;
		m->msec = ms;

		c->mouse.m[c->mouse.wi] = *m;
		if(++c->mouse.wi == nelem(c->mouse.m))
			c->mouse.wi = 0;
		if(c->mouse.wi == c->mouse.ri){
			c->mouse.stall = 1;
			c->mouse.ri = 0;
			c->mouse.wi = 1;
			c->mouse.m[0] = *m;
		}
		rwakeup(&bridge_mouserdz);
	}
	qunlock(&c->eventlk);
}

/*
 * Add ch to the keyboard buffer.
 * Must be called with c->eventlk held.
 */
static void
kputc(Client *c, int ch)
{
	if(canqlock(&c->eventlk)){
		fprint(2, "misuse of kputc\n");
		abort();
	}

	c->kbd.r[c->kbd.wi++] = ch;
	if(c->kbd.wi == nelem(c->kbd.r))
		c->kbd.wi = 0;
	if(c->kbd.ri == c->kbd.wi)
		c->kbd.stall = 1;
	rwakeup(&bridge_kbdrdz);
}

/*
 * Stop any pending compose sequence (mouse button clicked).
 * Called from the graphics thread with no locks held.
 */
void
gfx_abortcompose(Client *c)
{
	qlock(&c->eventlk);
	if(c->kbd.alting){
		c->kbd.alting = 0;
		c->kbd.nk = 0;
	}
	qunlock(&c->eventlk);
}

/*
 * Record a single-rune keystroke.
 * Called from the graphics thread with no locks held.
 */
void
gfx_keystroke(Client *c, int ch)
{
	int i;

	qlock(&c->eventlk);
	if(ch == Kalt){
		c->kbd.alting = !c->kbd.alting;
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	if(ch == Kcmd+'r'){
		if(c->forcedpi)
			c->forcedpi = 0;
		else if(c->displaydpi >= 200)
			c->forcedpi = 100;
		else
			c->forcedpi = 225;
		qunlock(&c->eventlk);
		c->impl->rpc_resizeimg(c);
		return;
	}
	if(!c->kbd.alting){
		kputc(c, ch);
		qunlock(&c->eventlk);
		return;
	}
	if(c->kbd.nk >= nelem(c->kbd.k))
		c->kbd.nk = 0;
	c->kbd.k[c->kbd.nk++] = ch;
	ch = latin1(c->kbd.k, c->kbd.nk);
	if(ch > 0){
		c->kbd.alting = 0;
		kputc(c, ch);
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	if(ch == -1){
		c->kbd.alting = 0;
		for(i=0; i<c->kbd.nk; i++)
			kputc(c, c->kbd.k[i]);
		c->kbd.nk = 0;
		qunlock(&c->eventlk);
		return;
	}
	/* need more input */
	qunlock(&c->eventlk);
}
