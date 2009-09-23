static const char font[]            = "-*-proggytiny-*-*-*-*-*-*-*-*-*-*-*-*";
static const char normbgcolor[]     = "#202020";
static const char normfgcolor[]     = "#c0c0c0";
static const char selbgcolor[]      = "#884400";
static const char selfgcolor[]      = "#f0f0f0";
static const int tabwidth           = 200;
static const char before[]          = "<";
static const char after[]           = ">";

#define EXEC "surf", "-e", winid
#define MODKEY ControlMask
static Key keys[] = { \
	/* modifier                     key        function        argument */
	{ MODKEY|ShiftMask,             XK_Return, spawn,          { .v = (char*[]){ EXEC, NULL} } },
	{ MODKEY|ShiftMask,             XK_t,      spawn,          { .v = (char*[]){ EXEC, NULL} } },
	{ MODKEY|ShiftMask,             XK_l,      rotate,         { .i = +1 } },
	{ MODKEY|ShiftMask,             XK_h,      rotate,         { .i = -1 } },
	{ MODKEY,                       XK_1,      move,           { .i = 1 } },
	{ MODKEY,                       XK_2,      move,           { .i = 2 } },
	{ MODKEY,                       XK_3,      move,           { .i = 3 } },
	{ MODKEY,                       XK_4,      move,           { .i = 4 } },
	{ MODKEY,                       XK_5,      move,           { .i = 5 } },
	{ MODKEY,                       XK_6,      move,           { .i = 6 } },
	{ MODKEY,                       XK_7,      move,           { .i = 7 } },
	{ MODKEY,                       XK_8,      move,           { .i = 8 } },
	{ MODKEY,                       XK_9,      move,           { .i = 9 } },
	{ MODKEY,                       XK_0,      move,           { .i = 10 } },
	{ MODKEY,                       XK_q,      killclient,     { 0 } },
};
