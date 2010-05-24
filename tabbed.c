/* See LICENSE file for copyright and license details.
 *
 * To understand tabbed, start reading main().
 */
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <locale.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <errno.h>

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY          0
#define XEMBED_WINDOW_ACTIVATE          1
#define XEMBED_WINDOW_DEACTIVATE        2
#define XEMBED_REQUEST_FOCUS            3
#define XEMBED_FOCUS_IN                 4
#define XEMBED_FOCUS_OUT                5
#define XEMBED_FOCUS_NEXT               6
#define XEMBED_FOCUS_PREV               7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON              10
#define XEMBED_MODALITY_OFF             11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT            0
#define XEMBED_FOCUS_FIRST              1
#define XEMBED_FOCUS_LAST               2

/* Macros */
#define MAX(a, b)                ((a) > (b) ? (a) : (b))
#define MIN(a, b)                ((a) < (b) ? (a) : (b))
#define LENGTH(x)                (sizeof x / sizeof x[0])
#define CLEANMASK(mask)          (mask & ~(numlockmask|LockMask))
#define TEXTW(x)                 (textnw(x, strlen(x)) + dc.font.height)

enum { ColFG, ColBG, ColLast };                         /* color */
enum { WMProtocols, WMDelete, WMLast };                 /* default atoms */

typedef union {
	int i;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	int x, y, w, h;
	unsigned long norm[ColLast];
	unsigned long sel[ColLast];
	Drawable drawable;
	GC gc;
	struct {
		int ascent;
		int descent;
		int height;
		XFontSet set;
		XFontStruct *xfont;
	} font;
} DC; /* draw context */

typedef struct Client {
	char name[256];
	struct Client *next;
	Window win;
	int tabx;
	Bool mapped;
	Bool closed;
} Client;

/* function declarations */
static void buttonpress(const XEvent *e);
static void cleanup(void);
static void clientmessage(const XEvent *e);
static void configurenotify(const XEvent *e);
static void configurerequest(const XEvent *e);
static void createnotify(const XEvent *e);
static void destroynotify(const XEvent *e);
static void die(const char *errstr, ...);
static void drawbar();
static void drawtext(const char *text, unsigned long col[ColLast]);
static void *emallocz(size_t size);
static void enternotify(const XEvent *e);
static void expose(const XEvent *e);
static void focus(Client *c);
static void focusin(const XEvent *e);
static void focusonce(const Arg *arg);
static Client *getclient(Window w);
static unsigned long getcolor(const char *colstr);
static Client *getfirsttab();
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void initfont(const char *fontstr);
static Bool isprotodel(Client *c);
static void keypress(const XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window win);
static void maprequest(const XEvent *e);
static void move(const Arg *arg);
static void propertynotify(const XEvent *e);
static void resize(Client *c, int w, int h);
static void rotate(const Arg *arg);
static void run(void);
static void sendxembed(Client *c, long msg, long detail, long d1, long d2);
static void setup(void);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static int textnw(const char *text, unsigned int len);
static void unmanage(Client *c);
static void updatenumlockmask(void);
static void updatetitle(Client *c);
static int xerror(Display *dpy, XErrorEvent *ee);

/* variables */
static int screen;
static void (*handler[LASTEvent]) (const XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureNotify] = configurenotify,
	[ConfigureRequest] = configurerequest,
	[CreateNotify] = createnotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
};
static int bh, wx, wy, ww, wh;
static unsigned int numlockmask = 0;
static Bool running = True, nextfocus;
static Display *dpy;
static DC dc;
static Atom wmatom[WMLast], xembedatom;
static Window root, win;
static Client *clients = NULL, *sel = NULL, *lastsel = NULL;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static char winid[64];
/* configuration, allows nested code to access above variables */
#include "config.h"

void
buttonpress(const XEvent *e) {
	const XButtonPressedEvent *ev = &e->xbutton;
	int i;
	Client *c;
	Arg arg;

	c = getfirsttab();
	if(c != clients && ev->x < TEXTW(before))
		return;
	for(i = 0; c; c = c->next, i++) {
		if(c->tabx > ev->x) {
			switch(ev->button) {
			case Button1:
				focus(c);
				break;
			case Button2:
				focus(c);
				killclient(NULL);
				break;
			case Button4:
			case Button5:
				arg.i = ev->button == Button4 ? -1 : 1;
				rotate(&arg);
				break;
			}
			break;
		}
	}
}

void
cleanup(void) {
	Client *c, *n;

	for(c = clients; c; c = n) {
		killclient(NULL);
		focus(c);
		XReparentWindow(dpy, c->win, root, 0, 0);
		n = c->next;
		unmanage(c);
	}
	if(dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	else
		XFreeFont(dpy, dc.font.xfont);
	XFreePixmap(dpy, dc.drawable);
	XFreeGC(dpy, dc.gc);
	XDestroyWindow(dpy, win);
	XSync(dpy, False);
}

void
clientmessage(const XEvent *e) {
	const XClientMessageEvent *ev = &e->xclient;

	if(ev->message_type == wmatom[WMProtocols]
			&& ev->data.l[0] == wmatom[WMDelete])
		running = False;
}

void
configurenotify(const XEvent *e) {
	const XConfigureEvent *ev = &e->xconfigure;

	if(ev->window == win && (ev->width != ww || ev->height != wh)) {
		ww = ev->width;
		wh = ev->height;
		XFreePixmap(dpy, dc.drawable);
		dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
		if(sel)
			resize(sel, ww, wh - bh);
		XSync(dpy, False);
	}
}

void
configurerequest(const XEvent *e) {
	const XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	Client *c;

	if((c = getclient(ev->window))) {
		wc.x = 0;
		wc.y = bh;
		wc.width = ww;
		wc.height = wh - bh;
		wc.border_width = 0;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, c->win, ev->value_mask, &wc);
	}
}

void
createnotify(const XEvent *e) {
	const XCreateWindowEvent *ev = &e->xcreatewindow;

	if(ev->window != win && !getclient(ev->window))
		manage(ev->window);
}

void
destroynotify(const XEvent *e) {
	const XDestroyWindowEvent *ev = &e->xdestroywindow;
	Client *c;

	if((c = getclient(ev->window)))
		unmanage(c);
}

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
drawbar() {
	unsigned long *col;
	int n, width;
	Client *c, *fc;
	char *name = NULL;

	if(!clients) {
		dc.x = 0;
		dc.w = ww;
		XFetchName(dpy, win, &name);
		drawtext(name ? name : "", dc.norm);
		XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, ww, bh, 0, 0);
		XSync(dpy, False);
		return;
	}
	width = ww;
	for(c = clients; c; c = c->next)
		c->tabx = -1;
	for(n = 0, fc = c = getfirsttab(); c; c = c->next, n++);
	if(n * tabwidth > width) {
		dc.w = TEXTW(after);
		dc.x = width - dc.w;
		drawtext(after, dc.sel);
		width -= dc.w;
	}
	dc.x = 0;
	if(fc != clients) {
		dc.w = TEXTW(before);
		drawtext(before, dc.sel);
		dc.x += dc.w;
		width -= dc.w;
	}
	for(c = fc; c && dc.x < width; c = c->next) {
		dc.w = tabwidth;
		if(c == sel) {
			col = dc.sel;
			if(n * tabwidth > width)
				dc.w += width % tabwidth;
			else
				dc.w = width - (n - 1) * tabwidth;
		}
		else {
			col = dc.norm;
		}
		drawtext(c->name, col);
		dc.x += dc.w;
		c->tabx = dc.x;
	}
	XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, ww, bh, 0, 0);
	XSync(dpy, False);
}

void
drawtext(const char *text, unsigned long col[ColLast]) {
	int i, x, y, h, len, olen;
	char buf[256];
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	XSetForeground(dpy, dc.gc, col[ColBG]);
	XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	if(!text)
		return;
	olen = strlen(text);
	h = dc.font.ascent + dc.font.descent;
	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
	x = dc.x + (h / 2);
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');
	XSetForeground(dpy, dc.gc, col[ColFG]);
	if(dc.font.set)
		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
	else
		XDrawString(dpy, dc.drawable, dc.gc, x, y, buf, len);
}

void *
emallocz(size_t size) {
	void *p;

	if(!(p = calloc(1, size)))
		die(0, "tabbed: cannot malloc");
	return p;
}

void
enternotify(const XEvent *e) {
	focus(sel);
}

void
expose(const XEvent *e) {
	const XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && win == ev->window)
		drawbar();
}

void
focus(Client *c) {
	/* If c, sel and clients are NULL, raise tabbed-win itself */
	if(!c && !(c = sel ? sel : clients)) {
		XStoreName(dpy, win, "tabbed-"VERSION);
		XRaiseWindow(dpy, win);
		return;
	}
	resize(c, ww, wh - bh);
	XRaiseWindow(dpy, c->win);
	sendxembed(c, XEMBED_FOCUS_IN, XEMBED_FOCUS_CURRENT, 0, 0);
	sendxembed(c, XEMBED_WINDOW_ACTIVATE, 0, 0, 0);
	XStoreName(dpy, win, c->name);
	if(sel != c) {
		lastsel = sel;
	}
	sel = c;
	drawbar();
}

void
focusin(const XEvent *e) {
	focus(sel);
}

void
focusonce(const Arg *arg) {
	nextfocus = True;
}

Client *
getclient(Window w) {
	Client *c;

	for(c = clients; c; c = c->next)
		if(c->win == w)
			return c;
	return NULL;
}

unsigned long
getcolor(const char *colstr) {
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if(!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		die("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

Client *
getfirsttab() {
	unsigned int n, seli;
	Client *c, *fc;

	if(!sel)
		return NULL;
	c = fc = clients;
	for(n = 0; c; c = c->next, n++);
	if(n * tabwidth > ww) {
		for(seli = 0, c = clients; c && c != sel; c = c->next, seli++);
		for(; seli * tabwidth > ww / 2 && n * tabwidth > ww;
				fc = fc->next, seli--, n--);
	}
	return fc;
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size) {
	char **list = NULL;
	int n;
	XTextProperty name;

	if(!text || size == 0)
		return False;
	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);
	if(!name.nitems)
		return False;
	if(name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return True;
}

void
initfont(const char *fontstr) {
	char *def, **missing;
	int i, n;

	missing = NULL;
	if(dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
	if(missing) {
		while(n--)
			fprintf(stderr, "tabbed: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
	}
	if(dc.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		dc.font.ascent = dc.font.descent = 0;
		font_extents = XExtentsOfFontSet(dc.font.set);
		n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
		for(i = 0, dc.font.ascent = 0, dc.font.descent = 0; i < n; i++) {
			dc.font.ascent = MAX(dc.font.ascent, (*xfonts)->ascent);
			dc.font.descent = MAX(dc.font.descent,(*xfonts)->descent);
			xfonts++;
		}
	}
	else {
		if(dc.font.xfont)
			XFreeFont(dpy, dc.font.xfont);
		dc.font.xfont = NULL;
		if(!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
		&& !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			die("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
}

Bool
isprotodel(Client *c) {
	int i, n;
	Atom *protocols;
	Bool ret = False;

	if(XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		for(i = 0; !ret && i < n; i++)
			if(protocols[i] == wmatom[WMDelete])
				ret = True;
		XFree(protocols);
	}
	return ret;
}

void
keypress(const XEvent *e) {
	const XKeyEvent *ev = &e->xkey;
	unsigned int i;
	KeySym keysym;

	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for(i = 0; i < LENGTH(keys); i++)
		if(keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient(const Arg *arg) {
	XEvent ev;

	if(!sel)
		return;
	if(isprotodel(sel) && !sel->closed) {
		ev.type = ClientMessage;
		ev.xclient.window = sel->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = wmatom[WMDelete];
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, sel->win, False, NoEventMask, &ev);
		sel->closed = True;
	}
	else
		XKillClient(dpy, sel->win);
}

void
manage(Window w) {
	updatenumlockmask();
	{
		int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;
		Client *c;
		XEvent e;

		XWithdrawWindow(dpy, w, 0);
		XReparentWindow(dpy, w, win, 0, bh);
		XSelectInput(dpy, w, PropertyChangeMask|StructureNotifyMask|EnterWindowMask);
		XSync(dpy, False);
		for(i = 0; i < LENGTH(keys); i++) {
			if((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for(j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], w,
						 True, GrabModeAsync, GrabModeAsync);
		}
		c = emallocz(sizeof(Client));
		c->next = clients;
		c->win = w;
		clients = c;
		updatetitle(c);
		drawbar();
		XMapRaised(dpy, w);
		e.xclient.window = w;
		e.xclient.type = ClientMessage;
		e.xclient.message_type = xembedatom;
		e.xclient.format = 32;
		e.xclient.data.l[0] = CurrentTime;
		e.xclient.data.l[1] = XEMBED_EMBEDDED_NOTIFY;
		e.xclient.data.l[2] = 0;
		e.xclient.data.l[3] = win;
		e.xclient.data.l[4] = 0;
		XSendEvent(dpy, root, False, NoEventMask, &e);
		XSync(dpy, False);
		focus(nextfocus ? c : sel);
		nextfocus = foreground;
		if(!lastsel)
			lastsel = c;
	}
}

void
maprequest(const XEvent *e) {
	const XMapRequestEvent *ev = &e->xmaprequest;

	if(!getclient(ev->window))
		manage(ev->window);
}

void
move(const Arg *arg) {
	int i;
	Client *c;

	for(i = 0, c = clients; c; c = c->next, i++) {
		if(arg->i == i)
			focus(c);
	}
}

void
propertynotify(const XEvent *e) {
	const XPropertyEvent *ev = &e->xproperty;
	Client *c;

	if(ev->state != PropertyDelete && ev->atom == XA_WM_NAME
			&& (c = getclient(ev->window))) {
		updatetitle(c);
	}
}

void
resize(Client *c, int w, int h) {
	XConfigureEvent ce;
	XWindowChanges wc;

	ce.x = 0;
	ce.y = bh;
	ce.width = wc.width = w;
	ce.height = wc.height = h;
	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.above = None;
	ce.override_redirect = False;
	ce.border_width = 0;
	XConfigureWindow(dpy, c->win, CWWidth|CWHeight, &wc);
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
rotate(const Arg *arg) {
	Client *c;

	if(arg->i == 0)
		focus(lastsel);
	else if(arg->i > 0) {
		if(sel && sel->next)
			focus(sel->next);
		else
			focus(clients);
	}
	else {
		for(c = clients; c && c->next && c->next != sel; c = c->next);
		if(c)
			focus(c);
	}
}

void
run(void) {
	XEvent ev;

	/* main event loop */
	XSync(dpy, False);
	drawbar();
	while(running) {
		XNextEvent(dpy, &ev);
		if(handler[ev.type])
			(handler[ev.type])(&ev); /* call handler */
	}
}

void
sendxembed(Client *c, long msg, long detail, long d1, long d2) {
	XEvent e = { 0 };

	e.xclient.window = c->win;
	e.xclient.type = ClientMessage;
	e.xclient.message_type = xembedatom;
	e.xclient.format = 32;
	e.xclient.data.l[0] = CurrentTime;
	e.xclient.data.l[1] = msg;
	e.xclient.data.l[2] = detail;
	e.xclient.data.l[3] = d1;
	e.xclient.data.l[4] = d2;
	XSendEvent(dpy, c->win, False, NoEventMask, &e);
}

void
setup(void) {
	/* clean up any zombies immediately */
	sigchld(0);
	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	initfont(font);
	bh = dc.h = dc.font.height + 2;
	/* init atoms */
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	xembedatom = XInternAtom(dpy, "_XEMBED", False);
	/* init appearance */
	wx = 0;
	wy = 0;
	ww = 800;
	wh = 600;
	dc.norm[ColBG] = getcolor(normbgcolor);
	dc.norm[ColFG] = getcolor(normfgcolor);
	dc.sel[ColBG] = getcolor(selbgcolor);
	dc.sel[ColFG] = getcolor(selfgcolor);
	dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	if(!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);

	win = XCreateSimpleWindow(dpy, root, wx, wy, ww, wh, 0, dc.norm[ColFG], dc.norm[ColBG]);
	XMapRaised(dpy, win);
	XSelectInput(dpy, win, SubstructureNotifyMask|FocusChangeMask|
			ButtonPressMask|ExposureMask|KeyPressMask|
			StructureNotifyMask|SubstructureRedirectMask);
	xerrorxlib = XSetErrorHandler(xerror);
	XClassHint class_hint;
	class_hint.res_name = "tabbed";
	class_hint.res_class = "Tabbed";
	XSetClassHint(dpy, win, &class_hint);
	XSetWMProtocols(dpy, win, &wmatom[WMDelete], 1);
	snprintf(winid, LENGTH(winid), "%u", (int)win);
	nextfocus = foreground;
	focus(clients);
}

void
sigchld(int unused) {
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
	if(fork() == 0) {
		if(dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "tabbed: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(0);
	}
}

int
textnw(const char *text, unsigned int len) {
	XRectangle r;

	if(dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}

void
unmanage(Client *c) {
	Client *pc;

	if(!clients)
		return;
	else if(c == clients)
		pc = clients = c->next;
	else {
		for(pc = clients; pc && pc->next && pc->next != c; pc = pc->next);
		pc->next = c->next;
	}
	if(c == lastsel)
		lastsel = clients;
	if(c == sel) {
		sel = pc;
		focus(lastsel);
	}
	free(c);
	drawbar();
	XSync(dpy, False);
}

void
updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for(i = 0; i < 8; i++)
		for(j = 0; j < modmap->max_keypermod; j++)
			if(modmap->modifiermap[i * modmap->max_keypermod + j]
			   == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatetitle(Client *c) {
	gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if(sel == c)
		XStoreName(dpy, win, c->name);
	drawbar();
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *ee) {
	if(ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "tabbed: fatal error: request code=%d, error code=%d\n",
			ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
main(int argc, char *argv[]) {
	int detach = 0;

	if(argc == 2 && !strcmp("-v", argv[1]))
		die("tabbed-"VERSION", © 2006-2008 tabbed engineers, see LICENSE for details\n");
	else if(argc == 2 && strcmp("-d", argv[1]) == 0)
		detach = 1;
	else if(argc != 1)
		die("usage: tabbed [-d] [-v]\n");
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");
	if(!(dpy = XOpenDisplay(0)))
		die("tabbed: cannot open display\n");
	setup();
	printf("%i\n", (int)win);
	fflush(NULL);
	if(detach) {
		if(fork() == 0)
			fclose(stdout);
		else {
			if(dpy)
				close(ConnectionNumber(dpy));
			return EXIT_SUCCESS;
		}
	}
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
