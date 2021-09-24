/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int gappih    = 20;       /* horiz inner gap between windows */
static const unsigned int gappiv    = 10;       /* vert inner gap between windows */
static const unsigned int gappoh    = 10;       /* horiz outer gap between windows and screen edge */
static const unsigned int gappov    = 30;       /* vert outer gap between windows and screen edge */
static       int smartgaps          = 0;        /* 1 means no outer gap when there is only one window */
static const int scalepreview       = 3;        /* Tag preview scaling */
static const unsigned int systraypinning = 0;   /* 0: sloppy systray follows selected monitor, >0: pin systray to monitor X */
static const unsigned int systrayonleft = 0;   	/* 0: systray in the right corner, >0: systray on left of status text */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const int systraypinningfailfirst = 1;   /* 1: if pinning fails, display systray on the first monitor, False: display systray on the last monitor*/
static const int showsystray        = 1;     /* 0 means no systray */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int startontag         = 0;        /* 0 means no tag active on start */
static const int user_bh            = 30;        /* 0 means that dwm will calculate bar height, >= 1 means dwm will user_bh as bar height */
#define ICONSIZE 16   /* icon size */
#define ICONSPACING 5 /* space between icon and title */
static const int vertpad            = 10;       /* vertical padding of bar */
static const int sidepad            = 10;       /* horizontal padding of bar */
static const int extrabarright      = 0;        /* 1 means extra bar text on right */
static const char statussep         = ';';      /* separator between status bars */
static const double activeopacity   = 1.0f;     /* Window opacity when it's focused (0 <= opacity <= 1) */
static const double inactiveopacity = 0.875f;   /* Window opacity when it's inactive (0 <= opacity <= 1) */
static const int horizpadbar        = 30;        /* horizontal padding for statusbar */
static const int vertpadbar         = 10;        /* vertical padding for statusbar */
static const char *fonts[]          = { "monospace:size=10", "Fontawesome:size=10" };
static const char dmenufont[]       = "monospace:size=10";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
	[SchemeHid]  = { col_cyan,  col_gray1, col_cyan  },
};
static const unsigned int alphas[][3]      = {
	/*               fg      bg        border     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha },
};
static const XPoint stickyicon[]    = { {0,0}, {4,0}, {4,8}, {2,6}, {0,8}, {0,0} }; /* represents the icon as an array of vertices */
static const XPoint stickyiconbb    = {4,8};	/* defines the bottom right corner of the polygon's bounding box (speeds up scaling) */

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
static const char *tagsalt[] = { "a", "b", "c", "d", "e", "6", "7", "8", "9" };
static const int momentaryalttags = 0; /* 1 means alttags will show only when key is held down*/

static const char ptagf[] = "[%s %s]";	/* format of a tag label */
static const char etagf[] = "[%s]";	/* format of an empty tag */
static const int lcaselbl = 0;		/* 1 means make tag label lowercase */	
static const char altptagf[] = "%s %s";	/* format of a tag label */
static const char altetagf[] = "%s";	/* format of an empty tag */
static const int altlcaselbl = 1;		/* 1 means make tag label lowercase */	

static const char *tagsel[][2] = {
	{ "#ffffff", "#ff0000" },
	{ "#ffffff", "#ff7f00" },
	{ "#000000", "#ffff00" },
	{ "#000000", "#00ff00" },
	{ "#ffffff", "#0000ff" },
	{ "#ffffff", "#4b0082" },
	{ "#ffffff", "#9400d3" },
	{ "#000000", "#ffffff" },
	{ "#ffffff", "#000000" },
};

static const unsigned int tagalpha[] = { OPAQUE, baralpha };

static const unsigned int ulinepad	= 5;	/* horizontal padding between the underline and tag */
static const unsigned int ulinestroke	= 2;	/* thickness / height of the underline */
static const unsigned int ulinevoffset	= 0;	/* how far above the bottom of the bar the line should appear */
static const int ulineall 		= 0;	/* 1 to show underline on all tags, 0 for just the active ones */

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor    scratch key */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1,        0  },
	{ "firefox",  NULL,       NULL,       1 << 8,       0,           -1,        0  },
	{ NULL,       NULL,   "scratchpad",   0,            1,           -1,       's' },
};

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = "👅";

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int decorhints  = 1;    /* 1 means respect decoration hints */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

#define FORCE_VSPLIT 1  /* nrowgrid layout: force two clients to always split vertically */

/* mouse scroll resize */
static const int scrollsensetivity = 30; /* 1 means resize window by 1 pixel for each scroll event */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "[M]",      monocle },
	{ "[@]",      spiral },
	{ "[\\]",     dwindle },
	{ "H[]",      deck },
	{ "TTT",      bstack },
	{ "===",      bstackhoriz },
	{ "HHH",      grid },
	{ "###",      nrowgrid },
	{ "---",      horizgrid },
	{ ":::",      gaplessgrid },
	{ "|M|",      centeredmaster },
	{ ">M>",      centeredfloatingmaster },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ NULL,       NULL },
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(CHAIN,KEY,TAG) \
	{ MODKEY,                       CHAIN,    KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           CHAIN,    KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             CHAIN,    KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, CHAIN,    KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

#define STATUSBAR "dwmblocks"

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
static const char *layoutmenu_cmd = "layoutmenu.sh";

/*First arg only serves to match against key in rules*/
static const char *scratchpadcmd[] = {"s", "st", "-t", "scratchpad", NULL}; 

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY|ControlMask,-1,           XK_space,  focusmaster,    {0} },
	{ MODKEY,-1,                       XK_grave,  togglescratch,  {.v = scratchpadcmd } },
	{ MODKEY,                       -1,XK_minus, scratchpad_show, {0} },
	{ MODKEY|ShiftMask,             -1,XK_minus, scratchpad_hide, {0} },
	{ MODKEY,                       -1,XK_equal,scratchpad_remove,{0} },
	{ MODKEY,                       -1,XK_o,      winview,        {0} },
	{ MODKEY|ShiftMask,             -1,XK_space,  togglealwaysontop, {0} },
	{ MODKEY|ControlMask,           -1,XK_j,      pushdown,       {0} },
	{ MODKEY|ControlMask,           -1,XK_k,      pushup,         {0} },
	{ MODKEY,                       -1,XK_s,      togglesticky,   {0} },
	{ MODKEY,                       -1,XK_o, shiftviewclients,    { .i = +1 } },
	{ MODKEY|ShiftMask,		-1,XK_h,      shiftboth,      { .i = -1 }	},
	{ MODKEY|ControlMask,		-1,XK_h,      shiftswaptags,  { .i = -1 }	},
	{ MODKEY|ControlMask,		-1,XK_l,      shiftswaptags,  { .i = +1 }	},
	{ MODKEY|ShiftMask,             -1,XK_l,      shiftboth,      { .i = +1 }	},
	{ MODKEY|ShiftMask,             -1,XK_o,	shiftview,         { .i = +1 } },
	{ MODKEY|ShiftMask,             -1,XK_i,	shiftview,         { .i = -1 } },
	{ MODKEY,	                -1,XK_i, shiftviewclients,    { .i = -1 } },
	{ MODKEY,                       -1,XK_k,      swalstopsel,    {0} },
	{ MODKEY,                       -1,XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             -1,XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       -1,XK_b,      togglebar,      {0} },
	{ MODKEY,                       -1,XK_e,      focusstackvis,  {.i = +1 } },
	{ MODKEY,                       -1,XK_u,      focusstackvis,  {.i = -1 } },
	{ MODKEY|ShiftMask,             -1,XK_e,      focusstackhid,  {.i = +1 } },
	{ MODKEY|ShiftMask,             -1,XK_u,      focusstackhid,  {.i = -1 } },
	{ MODKEY|ShiftMask,             -1,XK_z,      show,           {0} },
	{ MODKEY,                       -1,XK_z,      hide,           {0} },
	{ MODKEY,                       -1,XK_a,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       -1,XK_s,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       -1,XK_n,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       -1,XK_i,      setmfact,       {.f = +0.05} },
	{ MODKEY|ShiftMask,             -1,XK_h,      setcfact,       {.f = +0.25} },
	{ MODKEY|ShiftMask,             -1,XK_l,      setcfact,       {.f = -0.25} },
	{ MODKEY|ShiftMask,             -1,XK_o,      setcfact,       {.f =  0.00} },
	{ MODKEY,                       -1,XK_Return, zoom,           {0} },
	{ MODKEY|Mod4Mask,              -1,XK_u,      incrgaps,       {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_u,      incrgaps,       {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_i,      incrigaps,      {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_i,      incrigaps,      {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_o,      incrogaps,      {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_o,      incrogaps,      {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_6,      incrihgaps,     {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_6,      incrihgaps,     {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_7,      incrivgaps,     {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_7,      incrivgaps,     {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_8,      incrohgaps,     {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_8,      incrohgaps,     {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_9,      incrovgaps,     {.i = +1 } },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_9,      incrovgaps,     {.i = -1 } },
	{ MODKEY|Mod4Mask,              -1,XK_0,      togglegaps,     {0} },
	{ MODKEY|Mod4Mask|ShiftMask,    -1,XK_0,      defaultgaps,    {0} },
	{ MODKEY,                       -1,XK_Tab,    view,           {0} },
	{ MODKEY,                       -1,XK_g,      goback,         {0} },
	{ MODKEY|ShiftMask,             -1,XK_c,      killclient,     {0} },
	{ MODKEY|ShiftMask,             -1,XK_x,      killunsel,      {0} },
	{ MODKEY,                       -1,XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       -1,XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       -1,XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY|ControlMask,		-1,XK_comma,  cyclelayout,    {.i = -1 } },
	{ MODKEY|ControlMask,           -1,XK_period, cyclelayout,    {.i = +1 } },
	{ MODKEY,                       -1,XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             -1,XK_space,  togglefloating, {0} },
	{ MODKEY|ShiftMask,             -1,XK_f,      togglefullscr,  {0} },
	{ MODKEY,                       -1,XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             -1,XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       -1,XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       -1,XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             -1,XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             -1,XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       -1,XK_j,      togglealttag,   {0} },
	TAGKEYS(                        -1,XK_1,                      0)
	TAGKEYS(                        -1,XK_2,                      1)
	TAGKEYS(                        -1,XK_3,                      2)
	TAGKEYS(                        -1,XK_4,                      3)
	TAGKEYS(                        -1,XK_5,                      4)
	TAGKEYS(                        -1,XK_6,                      5)
	TAGKEYS(                        -1,XK_7,                      6)
	TAGKEYS(                        -1,XK_8,                      7)
	TAGKEYS(                        -1,XK_9,                      8)
	{ MODKEY|ShiftMask,             -1,XK_q,      quit,           {0} },
	{ MODKEY|ControlMask|ShiftMask, -1,XK_q,      quit,           {1} }, 
};

/* resizemousescroll direction argument list */
static const int scrollargs[][2] = {
	/* width change         height change */
	{ +scrollsensetivity,	0 },
	{ -scrollsensetivity,	0 },
	{ 0, 				  	+scrollsensetivity },
	{ 0, 					-scrollsensetivity },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        layoutmenu,     {0} },
	{ ClkTopTitle,          0,              Button1,        hide,      {0} },
	{ ClkWinTitle,          0,              Button1,        togglewin,      {0} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button1,        sigstatusbar,   {.i = 1} },
	{ ClkStatusText,        0,              Button2,        sigstatusbar,   {.i = 2} },
	{ ClkStatusText,        0,              Button3,        sigstatusbar,   {.i = 3} },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         MODKEY|ShiftMask, Button1,      swalmouse,      {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkClientWin,         MODKEY,         Button4,        resizemousescroll, {.v = &scrollargs[0]} },
	{ ClkClientWin,         MODKEY,         Button5,        resizemousescroll, {.v = &scrollargs[1]} },
	{ ClkClientWin,         MODKEY,         Button6,        resizemousescroll, {.v = &scrollargs[2]} },
	{ ClkClientWin,         MODKEY,         Button7,        resizemousescroll, {.v = &scrollargs[3]} },
};

/* signal definitions */
/* signum must be greater than 0 */
/* trigger signals using `xsetroot -name "fsignal:<signum>"` */
static Signal signals[] = {
	/* signum       function        argument  */
	{ 1,            setlayout,      {.v = 0} },
};

static const char *ipcsockpath = "/tmp/dwm.sock";
static IPCCommand ipccommands[] = {
  IPCCOMMAND(  view,                1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  toggleview,          1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  tag,                 1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  toggletag,           1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  tagmon,              1,      {ARG_TYPE_UINT}   ),
  IPCCOMMAND(  focusmon,            1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  focusstackvis,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  zoom,                1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  incnmaster,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  killclient,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  togglefloating,      1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  setmfact,            1,      {ARG_TYPE_FLOAT}  ),
  IPCCOMMAND(  setlayoutsafe,       1,      {ARG_TYPE_PTR}    ),
  IPCCOMMAND(  quit,                1,      {ARG_TYPE_NONE}   )
};

