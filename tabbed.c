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

/* macros */
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define LENGTH(x)       (sizeof x / sizeof x[0])
#define CLEANMASK(mask) (mask & ~(numlockmask|LockMask))
#define TEXTW(x)        (textnw(x, strlen(x)) + dc.font.height)

enum { ColFG, ColBG, ColLast };                         /* color */
enum { NetSupported, NetWMName, NetLast };              /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMLast };        /* default atoms */

typedef union {
	int i;
	unsigned int ui;
	float f;
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
} Client;

typedef struct Listener {
	int fd;
	struct Listener *next;
} Listener;

/* function declarations */
static void buttonpress(XEvent *e);
static void cleanup(void);
static void configurenotify(XEvent *e);
static void destroynotify(XEvent *e);
static void die(const char *errstr, ...);
static void drawbar();
static void drawtext(const char *text, unsigned long col[ColLast]);
static void expose(XEvent *e);
static void focus(Client *c);
static unsigned long getcolor(const char *colstr);
static Client *getclient(Window w);
static Client *getfirsttab();
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static Bool isprotodel(Client *c);
static void initfont(const char *fontstr);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void move(const Arg *arg);
static void spawntab(const Arg *arg);
static void manage(Window win);
static void propertynotify(XEvent *e);
static void resize(Client *c, int w, int h);
static void rotate(const Arg *arg);
static void run(void);
static void setup(void);
static int textnw(const char *text, unsigned int len);
static void unmanage(Client *c);
static void unmapnotify(XEvent *e);
static void updatenumlockmask(void);
static void updatetitle(Client *c);
static int xerror(Display *dpy, XErrorEvent *ee);

/* variables */
static int screen;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify,
	[ButtonPress] = buttonpress,
	[KeyPress] = keypress,
	[Expose] = expose,
};
static Display *dpy;
static DC dc;
static Atom wmatom[WMLast], netatom[NetLast];
static Window root, win;
static Bool running = True;
static unsigned int numlockmask = 0;
static unsigned bh, wx, wy, ww, wh;
static Client *clients = NULL, *sel = NULL;
static Listener *listeners;
static Bool badwindow = False;
/* configuration, allows nested code to access above variables */
#include "config.h"

void
buttonpress(XEvent *e) {
	int i;
	Client *c;
	XButtonPressedEvent *ev = &e->xbutton;

	c = getfirsttab();
	if(c != clients && ev->x < TEXTW(before))
		return;
	for(i = 0; c; c = c->next, i++) {
		if(c->tabx > ev->x) {
			focus(c);
			break;
		}
	}
}

void
cleanup(void) {
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
configurenotify(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;
	Client *c;

	if(ev->window == win && (ev->width != ww || ev->height != wh)) {
		ww = ev->width;
		wh = ev->height;
		XFreePixmap(dpy, dc.drawable);
		dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
		for(c = clients; c; c = c->next)
			resize(c, ww, wh - bh);
		XSync(dpy, False);
	}
}

void
destroynotify(XEvent *e) {
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

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

	if(!clients) {
		dc.x = 0;
		dc.w = ww;
		drawtext("Tabbed", dc.norm);
		return;
	}
	width = ww;
	for(c = clients; c; c = c->next)
		c->tabx = -1;
	for(n = 0, fc = c = getfirsttab(); c; c = c->next, n++);
	if(n * 200 > width) {
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
				dc.w = width - (n - 1) * 200;
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
	char buf[256];
	int i, x, y, h, len, olen;
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
		die(0, "Cannot Malloc");
	return p;
}

void
expose(XEvent *e) {
	XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && win == ev->window)
		drawbar();
}

void
focus(Client *c) {
	if(!c)
		c = clients;
	if(!c) {
		sel = NULL;
		return;
	}
	XRaiseWindow(dpy, c->win);
	XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XSelectInput(dpy, c->win, PropertyChangeMask|StructureNotifyMask);
	sel = c;
	XStoreName(dpy, win, sel->name);
	drawbar();
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
getclient(Window w) {
	Client *c;

	for(c = clients; c; c = c->next)
		if(c->win == w)
			return c;
	return NULL;
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
keypress(XEvent *e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
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
	if(isprotodel(sel)) {
		ev.type = ClientMessage;
		ev.xclient.window = sel->win;
		ev.xclient.message_type = wmatom[WMProtocols];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = wmatom[WMDelete];
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, sel->win, False, NoEventMask, &ev);
	}
	else
		XKillClient(dpy, sel->win);
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
sigchld(int signal) {
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void
spawntab(const Arg *arg) {
	int fd[2];
	Listener *l;

	if(pipe(fd)) {
		perror("tabbed: pipe failed");
		return;
	}
	l = emallocz(sizeof(Listener));
	l->fd = fd[0];
	l->next = listeners;
	listeners = l;
	signal(SIGCHLD, sigchld);
	if(fork() == 0) {
		if(dpy)
			close(ConnectionNumber(dpy));
		setsid();
		dup2(fd[1], STDOUT_FILENO);
		close(fd[0]);
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "tabbed: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(0);
	}
	close(fd[1]);
}

void
manage(Window w) {
	updatenumlockmask();
	{
		int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;
		Client *c;

		XSync(dpy, False);
		XReparentWindow(dpy, w, win, 0, bh);
		if(badwindow) {
			badwindow = False;
			return;
		}
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
		focus(c);
		updatetitle(c);
		resize(c, ww, wh - bh);
		drawbar();
	}
}

void
propertynotify(XEvent *e) {
	Client *c;
	XPropertyEvent *ev = &e->xproperty;

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
	XSync(dpy, False);
}

void
rotate(const Arg *arg) {
	Client *c;

	if(arg->i > 0) {
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
	char buf[32], *p;
	fd_set rd;
	int r, xfd, maxfd, wid;
	unsigned int offset = 0;
	XEvent ev;
	Listener *l, *pl;

	/* main event loop, also reads xids from stdin */
	XSync(dpy, False);
	xfd = ConnectionNumber(dpy);
	buf[LENGTH(buf) - 1] = '\0'; /* 0-terminator is never touched */
	drawbar();
	while(running) {
		FD_ZERO(&rd);
		maxfd = xfd;
		FD_SET(xfd, &rd);
		for(l = listeners; l; l = l->next) {
			maxfd = MAX(maxfd, l->fd);
			FD_SET(l->fd, &rd);
		}
		if(select(maxfd + 1, &rd, NULL, NULL, NULL) == -1) {
			if(errno == EINTR)
				continue;
			die("select failed\n");
		}
		for(l = listeners; l; l = l->next) {
			if(!FD_ISSET(l->fd, &rd))
				continue;
			switch((r = read(l->fd, buf + offset, LENGTH(buf) - 1 - offset))) {
			case -1:
				perror("tabbed: fd error");
			case 0:
				close(l->fd);
				if(listeners == l)
					listeners = l->next;
				else {
					for(pl = listeners; pl->next != l ; pl = pl->next);
					pl->next = l->next;
				}
				free(l);
				break;
			default:
				for(p = buf + offset; r > 0; p++, r--, offset++)
					if(*p == '\n' || *p == '\0') {
						*p = '\0';
						if((wid = atoi(buf)))
							manage((Window)wid);
						p += r - 1; /* p is buf + offset + r - 1 */
						for(r = 0; *(p - r) && *(p - r) != '\n'; r++);
						offset = r;
						if(r)
							memmove(buf, p - r + 1, r);
						break;
					}
				break;
			}
		}
		while(XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if(handler[ev.type])
				(handler[ev.type])(&ev); /* call handler */
		}
	}
}

void
setup(void) {
	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	initfont(font);
	bh = dc.h = dc.font.height + 2;
	/* init atoms */
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
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
	XSelectInput(dpy, win, StructureNotifyMask|PointerMotionMask|
			ButtonPressMask|ExposureMask|KeyPressMask|
			LeaveWindowMask);
	XMapRaised(dpy, win);
	XSetErrorHandler(xerror);
	XClassHint class_hint;
	XStoreName(dpy, win, "Tabbed");
	class_hint.res_name = "tabbed";
	class_hint.res_class = "Tabbed";
	XSetClassHint(dpy, win, &class_hint);
	listeners = emallocz(sizeof(Listener));
	listeners->fd = STDIN_FILENO;
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
unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = getclient(ev->window)))
		unmanage(c);
	else if(ev->window == win)
		running = False;
}

void
unmanage(Client *c) {
	Client *pc;

	focus(NULL);
	if(!clients)
		return;
	else if(c == clients)
		clients = c->next;
	else {
		for(pc = clients; pc && pc->next && pc->next != c; pc = pc->next);
		pc->next = c->next;
	}
	free(c);
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
	if(ee->error_code == BadWindow) {
		badwindow = True;
		puts("badwindow");
		return 0;
	}
	die("tabbed: fatal error: request code=%d, error code=%d\n",
			ee->request_code, ee->error_code);
	return 1;
}

int
main(int argc, char *argv[]) {
	if(argc == 2 && !strcmp("-v", argv[1]))
		die("tabbed-"VERSION", © 2006-2008 surf engineers, see LICENSE for details\n");
	else if(argc != 1)
		die("usage: tabbed [surf-options]\n");
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");
	if(!(dpy = XOpenDisplay(0)))
		die("tabbed: cannot open display\n");
	setup();
	run();
	/*dummys*/
	cleanup();
	XCloseDisplay(dpy);
	return 0;
	textnw(" ", 1);
	updatenumlockmask();
}
