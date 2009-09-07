/* See LICENSE file for copyright and license details.
 *
 * To understand tabbed, start reading main().
 */
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/XTest.h>

/* macros */
#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define LENGTH(x)       (sizeof x / sizeof x[0])
#define CLEANMASK(mask) (mask & ~(numlockmask|LockMask))

enum { ColFG, ColBG, ColLast };              /* color */

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
} Client;

typedef struct Listener {
	int fd;
	struct Listener *next;
} Listener;

/* function declarations */
static void cleanup(void);
static void configurenotify(XEvent *e);
static void unmapnotify(XEvent *e);
static void die(const char *errstr, ...);
static void expose(XEvent *e);
static unsigned long getcolor(const char *colstr);
static void initfont(const char *fontstr);
static void keypress(XEvent *e);
static void move(const Arg *arg);
static void newtab(const Arg *arg);
static void rotate(const Arg *arg);
static void run(void);
static void setup(void);
static int textnw(const char *text, unsigned int len);
static void updatenumlockmask(void);

/* variables */
static int screen;
static int wx, wy, ww, wh;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ConfigureNotify] = configurenotify,
	[UnmapNotify] = unmapnotify,
	[KeyPress] = keypress,
	[Expose] = expose,
};
static Display *dpy;
static DC dc;
static Window root, win;
static Bool running = True;
static unsigned int numlockmask = 0;
/* configuration, allows nested code to access above variables */
#define TABBED
#include "config.h"

void
buttonrelease(XEvent *e) {
	//XButtonPressedEvent *ev = &e->xbutton;
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
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
configurenotify(XEvent *e) {
	XConfigureEvent *ev = &e->xconfigure;

	if(ev->window == win && (ev->width != ww || ev->height != wh)) {
		ww = ev->width;
		wh = ev->height;
		XFreePixmap(dpy, dc.drawable);
		dc.drawable = XCreatePixmap(dpy, root, ww, wh, DefaultDepth(dpy, screen));
	}
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
unmapnotify(XEvent *e) {
	running = False;
}

void
expose(XEvent *e) {
	/*XExposeEvent *ev = &e->xexpose;*/
}

unsigned long
getcolor(const char *colstr) {
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if(!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		die("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
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
move(const Arg *arg) {
	puts("move to nth tab");
}

void
newtab(const Arg *arg) {
	puts("opening new tab");
}

void
rotate(const Arg *arg) {
	puts("next/prev tab");
}

void
run(void) {
	XEvent ev;

	XSync(dpy, False);
	while(running) {
		XNextEvent(dpy, &ev);
		if(handler[ev.type])
			(handler[ev.type])(&ev); /* call handler */
	}
}

void
setup(void) {
	XWMHints *wmh;

	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	initfont(font);

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
	wmh = XAllocWMHints();
	wmh->input = False;
	wmh->flags = InputHint;
	XSetWMHints(dpy, win, wmh);
	XFree(wmh);
	XMapRaised(dpy, win);
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
	textnw(surfexec[0], strlen(surfexec[0]));
	updatenumlockmask();
}
