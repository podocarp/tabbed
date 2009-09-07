#ifdef SURF
/* modifier 0 means no modifier */
static GdkColor progress       = { 65535,65535,0,0 };
static GdkColor progress_trust = { 65535,0,65535,0 };
#define MODKEY GDK_CONTROL_MASK
static Key keys[] = {
    /* modifier	            keyval      function        arg             Focus */
    { MODKEY,               GDK_R,      reload,         {.b = TRUE},    ALWAYS },
    { MODKEY,               GDK_r,      reload,         {.b = FALSE},   ALWAYS },
    { MODKEY,               GDK_g,      showurl,        {0},            ALWAYS },
    { MODKEY,               GDK_slash,  showsearch,     {0},            ALWAYS },
    { 0,                    GDK_Escape, hidesearch,     {0},            ALWAYS },
    { 0,                    GDK_Escape, hideurl,        {0},            ALWAYS },
    { MODKEY,               GDK_P,      print,          {0},            ALWAYS },
    { MODKEY,               GDK_p,      clipboard,      {.b = TRUE },   BROWSER },
    { MODKEY,               GDK_y,      clipboard,      {.b = FALSE},   BROWSER },
    { MODKEY,               GDK_j,      zoom,           {.i = +1 },     BROWSER },
    { MODKEY,               GDK_k,      zoom,           {.i = -1 },     BROWSER },
    { MODKEY,               GDK_i,      zoom,           {.i = 0 },      BROWSER },
    { MODKEY,               GDK_l,      navigate,       {.i = +1},      BROWSER },
    { MODKEY,               GDK_h,      navigate,       {.i = -1},      BROWSER },
    { 0,                    GDK_Escape, stop,           {0},            BROWSER },
    { MODKEY,               GDK_n,      searchtext,     {.b = TRUE},    BROWSER|SEARCHBAR },
    { MODKEY,               GDK_N,      searchtext,     {.b = FALSE},   BROWSER|SEARCHBAR },
    { 0,                    GDK_Return, searchtext,     {.b = TRUE},    SEARCHBAR },
    { GDK_SHIFT_MASK,       GDK_Return, searchtext,     {.b = FALSE},   SEARCHBAR },
    { 0,                    GDK_Return, loaduri,        {.v = NULL},    URLBAR },
    { 0,                    GDK_Return, hideurl,        {0},            URLBAR },
};
#else

static const char font[]            = "-*-proggytiny-*-*-*-*-*-*-*-*-*-*-*-*";
static const char normbgcolor[]     = "#202020";
static const char normfgcolor[]     = "#c0c0c0";
static const char selbgcolor[]      = "#884400";
static const char selfgcolor[]      = "#f0f0f0";
static const char *surfexec[]       = { "surf", "-x" };

#define MODKEY ControlMask
Key keys[] = { \
	/* modifier                     key        function        argument */
	{ MODKEY|ShiftMask,             XK_Return, newtab,         { 0 } },
	{ MODKEY,                       XK_t,      newtab,         { 0 } },
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
};
#endif
