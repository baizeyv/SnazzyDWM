/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */

static const unsigned int snap      = 64;       /* snap pixel */

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

static const char slopspawnstyle[]  = "-t 0 -c 0.92,0.85,0.69,0.3 -o"; /* do NOT define -f (format) here */
static const char slopresizestyle[] = "-t 0 -c 0.92,0.85,0.69,0.3"; /* do NOT define -f (format) here */
static const int riodraw_borders    = 0;        /* 0 or 1, indicates whether the area drawn using slop includes the window borders */
static const int riodraw_matchpid   = 1;        /* 0 or 1, indicates whether to match the PID of the client that was spawned with riospawn */
static const int riodraw_spawnasync = 0;        /* 0 means that the application is only spawned after a successful selection while
                                                 * 1 means that the application is being initialised in the background while the selection is made */

static const Bool viewontag         = True;     /* Switch view on tag switch */

/*  Display modes of the tab bar: never shown, always shown, shown only in  */
/*  monocle mode in the presence of several windows.                        */
/*  Modes after showtab_nmodes are disabled.                                */
enum showtab_modes { showtab_never, showtab_auto, showtab_nmodes, showtab_always};
static const int showtab			= showtab_auto;        /* Default tab bar show mode */
static const int toptab				= True;               /* False means bottom tab bar */
static const int tabclientgap	= 5;
static const char *btn_prev = "";
static const char *btn_next = "";
static const char *btn_close = "";
static const char *tabstatus = "TAB STATUS! --BAIZEYV";
static const int tabstatuscenter = 1;

/*
0 - master (default behaviour): new windows become the new master
1 - attachabove: new window is placed above selected client
2 - attachaside: new window is placed on top of the stack
3 - attachbelow: new window is placed below selected client
4 - attachbottom: new window is placed at the bottom of the stack
*/
static int attachmode         = 3;        /* 0 master (default), 1 = above, 2 = aside, 3 = below, 4 = bottom */

static const int startontag         = 0;        /* 0 means no tag active on start */

static const int user_bh            = 30;        /* 0 means that dwm will calculate bar height, >= 1 means dwm will user_bh as bar height */

#define ICONSIZE 16   /* icon size */
#define ICONSPACING 5 /* space between icon and title */

static const int vertpad            = 10;       /* vertical padding of bar */
static const int sidepad            = 10;       /* horizontal padding of bar */

static const int extrabarright      = 1;        /* 1 means extra bar text on right */
static const char statussep         = ';';      /* separator between status bars */

static const double activeopacity   = 1.0f;     /* Window opacity when it's focused (0 <= opacity <= 1) */
static const double inactiveopacity = 0.875f;   /* Window opacity when it's inactive (0 <= opacity <= 1) */

static const int horizpadbar        = 30;        /* horizontal padding for statusbar */
static const int vertpadbar         = 10;        /* vertical padding for statusbar */

static const char *fonts[]          = { "monospace:size=10", "Fontawesome:size=10" };
static const char dmenufont[]       = "monospace:size=10";

static const unsigned int baralpha = 0xd0;
static const unsigned int borderalpha = OPAQUE;

static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const char normmarkcolor[]   = "#775500";	/*border color for marked client*/
static const char selmarkcolor[]    = "#775577";	/*border color for marked client on focus*/
static const char closefgcolor[]       = "#eeeeee";
static const char prevfgcolor[]       = "#eeeeee";
static const char nextfgcolor[]       = "#bbbbbb";
static const char closebgcolor[]       = "#37474F";
static const char prevbgcolor[]       = "#37474F";
static const char nextbgcolor[]       = "#222222";
static const char *colors[][4]      = {
	/*               fg         bg         border		mark   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2,	normmarkcolor },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan,	selmarkcolor  },
	[SchemeHid]  = { col_cyan,  col_gray1, col_cyan,	normmarkcolor  },
	[SchemeClose]  = { closefgcolor, closebgcolor,  col_gray2,	selmarkcolor  },
	[SchemePrev]  = { prevfgcolor, prevbgcolor,  col_gray2,	selmarkcolor  },
	[SchemeNext]  = { nextfgcolor, nextbgcolor,  col_gray2,	selmarkcolor  },
};
static const unsigned int alphas[][4]      = {
	/*               fg      bg        border	mark     */
	[SchemeNorm] = { OPAQUE, baralpha, borderalpha,borderalpha },
	[SchemeSel]  = { OPAQUE, baralpha, borderalpha,borderalpha },
	[SchemeClose]  = { OPAQUE, baralpha, borderalpha, borderalpha },
	[SchemePrev]   = { OPAQUE, baralpha, borderalpha, borderalpha },
	[SchemeNext]   = { OPAQUE, baralpha, borderalpha, borderalpha },
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

/* grid of tags */
#define DRAWCLASSICTAGS             1 << 0
#define DRAWTAGGRID                 1 << 1

#define SWITCHTAG_UP                1 << 0
#define SWITCHTAG_DOWN              1 << 1
#define SWITCHTAG_LEFT              1 << 2
#define SWITCHTAG_RIGHT             1 << 3
#define SWITCHTAG_TOGGLETAG         1 << 4
#define SWITCHTAG_TAG               1 << 5
#define SWITCHTAG_VIEW              1 << 6
#define SWITCHTAG_TOGGLEVIEW        1 << 7

static const unsigned int drawtagmask = DRAWTAGGRID; /* | DRAWCLASSICTAGS to show classic row of tags */
static const int tagrows = 3;

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 *  	WM_WINDOW_ROLE(STRING) = role
	 */

	/* unmanaged  'conky can use this'
	As such the value of the unmanged rule plays a part:
		0 - the window is managed by the window manager
		1 - the window will be placed above all other windows
		2 - the window will be placed below all other windows
		3 - the window is left as-is (neither lowered nor raised)
   	*/

	/* switchtag
	This patch adds an extra configuration option for individual rules where:
		- 0 is default behaviour
		- 1 automatically moves you to the tag of the newly opened application and
		- 2 enables the tag of the newly opened application in addition to your existing enabled tags
		- 3 as 1, but closing that window reverts the view back to what it was previously (*)
		- 4 as 2, but closing that window reverts the view back to what it was previously (*)
	 */

	/* class	role       instance    title       tags mask     isfloating   monitor    scratch key	canfocus	float x,y,w,h	floatborderpx	unmanaged	switchtag	iscentered */
	{ "Gimp",	NULL,      NULL,       NULL,       0,            1,           -1,        0,		1,		50,50,500,500,	5,		0,		0,		0  },
	{ "firefox",	"browser", NULL,       NULL,       1 << 8,       0,           -1,        0,		1,		50,50,500,500,	5,		0,		3,		0  },
	{ NULL,		NULL,      NULL,   "scratchpad",   0,            1,           -1,       's',		1,		50,50,800,500,	-1,		0,		0,		1 },
};

/* window swallowing */
static const int swaldecay = 3;
static const int swalretroactive = 1;
static const char swalsymbol[] = "👅";

static const MonitorRule monrules[] = {
	/* monitor layout  mfact  nmaster  showbar  topbar */
	{  1,      2,      -1,    -1,      -1,      -1     }, // use a different layout for the second monitor
	{  -1,     1,      -1,    -1,      -1,      -1     }, // default
};

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
#define ALT Mod1Mask
#define SUPER Mod4Mask
#define CTRL ControlMask
#define SHIFT ShiftMask

#define TAGKEYS(CHAIN,KEY,TAG) \
	{ ALT,               	CHAIN,    KEY,      view,           {.ui = 1 << TAG} }, \
	{ SUPER,               	CHAIN,    KEY,      comboview,           {.ui = 1 << TAG} }, \
	{ SUPER|SHIFT,          CHAIN,    KEY,      combotag,           {.ui = 1 << TAG} }, \
	{ ALT|CTRL,          	CHAIN,    KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ ALT|SHIFT,         	CHAIN,    KEY,      tag,            {.ui = 1 << TAG} }, \
	{ CTRL|SHIFT,        	CHAIN,    KEY,      tagto,          {.ui = 1 << TAG} }, \
	{ ALT|CTRL|SHIFT, 	CHAIN,    KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

#define STATUSBAR "dwmblocks"

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "st", NULL };
static const char *layoutmenu_cmd = "/home/baizeyv/.local/std.app/baizeyv.SnazzyDWM/layoutmenu.sh";

/*First arg only serves to match against key in rules*/
static const char *scratchpadcmd[] = {"s", "st", "-t", "scratchpad", NULL}; 

static Key keys[] = {
	/* modifier				chain key	key		function			argument		summary */
	{ SUPER,		             	-1,		XK_Return, 	spawn,          		{.v = termcmd } }, /* terminal command */
	{ ALT,		                       	-1,		XK_Return, 	zoom,           		{0} 		}, /* zoom command */
	{ SUPER,	                       	-1,		XK_0,      	view,           		{.ui = ~0 } },
	{ SUPER,	                       	-1,		XK_o,      	winview,        		{0} 		}, /* winview command */
	{ SUPER,				-1,           	XK_space,  	focusmaster,    		{0} 		}, /* focus master command */
	{ SUPER|SHIFT,	           		-1,		XK_e,      	pushdown,       		{0} 		}, /* push down command */
	{ SUPER|SHIFT,	           		-1,		XK_u,      	pushup,         		{0} 		}, /* push up command */
	{ ALT,					-1,             XK_grave,  	togglescratch,  		{.v = scratchpadcmd } }, /* scratch command */
	{ ALT|CTRL,	                        -1,		XK_n,      	setmfact,       		{.f = -0.05} 	}, /* decrease mfact */
	{ ALT|CTRL,	                        -1,		XK_i,      	setmfact,       		{.f = +0.05} 	}, /* increase mfact */
	{ ALT|CTRL,		             	-1,		XK_u,      	setcfact,       		{.f = +0.25} 	}, /* increase cfact */
	{ ALT|CTRL,		             	-1,		XK_e,      	setcfact,       		{.f = -0.25} 	}, /* decrease cfact */
	{ ALT|CTRL,		             	-1,		XK_o,      	setcfact,       		{.f =  0.00} 	}, /* default cfact */
	{ SUPER,				-1,		XK_comma,  	cyclelayout,    		{.i = -1 } 	}, /* previous layout */
	{ SUPER,           			-1,		XK_period, 	cyclelayout,    		{.i = +1 } 	}, /* next layout */
	{ SUPER,	                       	-1,		XK_Tab,      	goback,         		{0} 		}, /* goback command */
	{ ALT,		                       	-1,		XK_Tab,    	view,           		{0} },
	{ SUPER,	                       	XK_semicolon,	XK_semicolon,   spawn,          		{.v = dmenucmd }}, /* dmenu command */
	{ SUPER,                       		XK_semicolon,	XK_c, 		scratchpad_show, 		{0} 		},
	{ SUPER,             			XK_semicolon,	XK_z, 		scratchpad_hide, 		{0} 		},
	{ SUPER,                       		XK_semicolon,	XK_x, 		scratchpad_remove,		{0} 		},
	{ SUPER,	             		XK_c,		XK_c,      	killclient,     		{0} 		}, /* kill command */
	{ SUPER,	             		XK_c,		XK_x,      	killunsel,      		{0} 		}, /* kill unsel command */
	{ SUPER,				XK_r,           XK_t,      	reorganizetags, 		{0} 		}, /* reorganize tags command */
	{ SUPER,				XK_r, 		XK_d, 		distributetags, 		{0} 		}, /* distribute tags command */
	{ ALT|SHIFT,		             	XK_q,		XK_x,      	quit,           		{0} 		}, /* quit command */
	{ ALT|SHIFT, 				XK_q,		XK_r,      	quit,           		{1} 		}, /* restart command */
	{ SUPER,	                       	XK_t,		XK_a,      	togglealttag,   		{0} 		}, /* alt tag command */
    	{ SUPER,           			XK_t,		XK_Tab,     	toggleattachx,      		{0} 		}, /* attachx command */
	{ SUPER,				XK_t,           XK_m,      	tabmode,        		{-1} 		}, /* tabmode command */
	{ SUPER,	             		XK_t,		XK_f,  		togglefloating, 		{0} 		}, /* toggle float command */
	{ SUPER,	                       	XK_t,		XK_s,      	togglesticky,   		{0} 		}, /* toggle sticky command */
	{ SUPER,	             		XK_t,		XK_w,  		togglealwaysontop, 		{0} 		}, /* toggle top command */
    	{ SUPER,	                       	XK_m,		XK_m, 		togglemark,   			{0} 		}, /* toggle mark command */
    	{ SUPER,	                       	XK_m,		XK_f,      	swapfocus,      		{0} 		}, /* swap mark focus command */
    	{ SUPER,	                       	XK_m,		XK_s,      	swapclient,     		{0} 		}, /* swap mark client command */
	{ SUPER,	                       	XK_b,		XK_b,      	togglebar,      		{0} 		}, /* toggle bar command */
	{ SUPER,				XK_f,           XK_c,      	togglefloatcenter,   		{0} 		}, /* toggle float center command */
	{ SUPER,				XK_f,           XK_f,      	togglefullscr,  		{0} 		}, /* toggle full screen */
  	{ SUPER,				XK_f,           XK_s,      	togglecanfocusfloating,   	{0} 		}, /* can focus float command */
	{ SUPER,	             		XK_z,		XK_s,      	show,           		{0} 		}, /* show command */
	{ SUPER,	                       	XK_z,		XK_z,      	hide,           		{0} 		}, /* hide command */
	{ SUPER,	                       	XK_s,		XK_t,      	swalstopsel,    		{0} 		}, /* swal stop command */
	{ SUPER,                       		XK_a,		XK_a,      	incnmaster,     		{.i = +1 } 	},
	{ SUPER,                       		XK_a,		XK_s,      	incnmaster,     		{.i = -1 } 	},
	{ ALT|CTRL,				-1,             XK_Right,      	aspectresize,   		{.i = +24} 	},
	{ ALT|CTRL,				-1,             XK_Left,      	aspectresize,   		{.i = -24} 	},
	{ ALT,		                       	-1,		XK_Down,   	moveresize,     		{.v = "0x 25y 0w 0h" }  }, /* move down */
	{ ALT,		                       	-1,		XK_Up,     	moveresize,     		{.v = "0x -25y 0w 0h" } }, /* move up */
	{ ALT,		                       	-1,		XK_Right,  	moveresize,     		{.v = "25x 0y 0w 0h" }  }, /* move right */
	{ ALT,		                       	-1,		XK_Left,   	moveresize,     		{.v = "-25x 0y 0w 0h" } }, /* move left */
	{ SUPER,             			-1,		XK_Down,   	moveresize,     		{.v = "0x 0y 0w 25h" } }, /* increase height */
	{ SUPER,             			-1,		XK_Up,     	moveresize,     		{.v = "0x 0y 0w -25h" } }, /* decrease height */
	{ SUPER,             			-1,		XK_Right,  	moveresize,     		{.v = "0x 0y 25w 0h" } }, /* increase width */
	{ SUPER,             			-1,		XK_Left,   	moveresize,     		{.v = "0x 0y -25w 0h" } }, /* decrease width */
	{ ALT|SUPER,           			-1,		XK_Up,     	moveresizeedge, 		{.v = "t"} },
	{ ALT|SUPER,           			-1,		XK_Down,   	moveresizeedge, 		{.v = "b"} },
	{ ALT|SUPER,           			-1,		XK_Left,   	moveresizeedge, 		{.v = "l"} },
	{ ALT|SUPER,           			-1,		XK_Right,  	moveresizeedge, 		{.v = "r"} },
	{ SUPER|CTRL, 				-1,		XK_Up,     	moveresizeedge, 		{.v = "T"} },
	{ SUPER|CTRL, 				-1,		XK_Down,   	moveresizeedge, 		{.v = "B"} },
	{ SUPER|CTRL, 				-1,		XK_Left,   	moveresizeedge, 		{.v = "L"} },
	{ SUPER|CTRL, 				-1,		XK_Right,  	moveresizeedge, 		{.v = "R"} },
	{ SUPER,	                       	-1,		XK_e,      	focusstackvis,  		{.i = +1 } },
	{ SUPER,	                       	-1,		XK_u,      	focusstackvis,  		{.i = -1 } },
	{ ALT,                       		-1,		XK_comma,  	focusmon,       		{.i = -1 } },
	{ ALT,                       		-1,		XK_period, 	focusmon,       		{.i = +1 } },
	{ ALT|SHIFT,             		-1,		XK_comma,  	tagmon,         		{.i = -1 } },
	{ ALT|SHIFT,             		-1,		XK_period, 	tagmon,         		{.i = +1 } },
	{ ALT|SUPER,             		-1,		XK_e,      	focusstackhid,  		{.i = +1 } },
	{ ALT|SUPER,             		-1,		XK_u,      	focusstackhid,  		{.i = -1 } },
	{ SUPER,             			-1,		XK_bracketleft,	setborderpx,    		{.i = -1 } },
	{ SUPER,             			-1,		XK_bracketright,setborderpx,    		{.i = +1 } },
	{ SUPER,             			-1,		XK_backslash, 	setborderpx,    		{.i = 0 } },
	{ ALT,             			-1,		XK_i,		shiftview,         		{ .i = +1 } },
	{ ALT,             			-1,		XK_n,		shiftview,         		{ .i = -1 } },
    	{ ALT|SHIFT,              		-1,		XK_i,  		switchtags,      		{ .ui = SWITCHTAG_RIGHT  | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    	{ ALT|SHIFT,              		-1,		XK_n,   	switchtags,      		{ .ui = SWITCHTAG_LEFT   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
	{ SUPER,                       		-1,		XK_i, 		shiftviewclients,    		{ .i = +1 } },
	{ SUPER,                       		-1,		XK_n, 		shiftviewclients,    		{ .i = -1 } },
	{ SUPER|SHIFT,                     	-1,		XK_i, 		shifttagclients,    		{ .i = +1 } },
	{ SUPER|SHIFT,                     	-1,		XK_n, 		shifttagclients,    		{ .i = -1 } },
    	{ ALT,           			-1,		XK_u,     	switchtags,     		{ .ui = SWITCHTAG_UP     | SWITCHTAG_VIEW } },
    	{ ALT,           			-1,		XK_e,   	switchtags,     		{ .ui = SWITCHTAG_DOWN   | SWITCHTAG_VIEW } },
    	{ ALT|SHIFT,              		-1,		XK_u,     	switchtags,     		{ .ui = SWITCHTAG_UP     | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
    	{ ALT|SHIFT,              		-1,		XK_e,   	switchtags,     		{ .ui = SWITCHTAG_DOWN   | SWITCHTAG_TAG | SWITCHTAG_VIEW } },
	{ SUPER|CTRL,             		-1,		XK_e,      	inplacerotate,  		{.i = +1} },
	{ SUPER|CTRL,             		-1,		XK_u,      	inplacerotate,  		{.i = -1} },
	{ ALT|SUPER,              		-1,		XK_0,      	togglegaps,     		{0} },
	{ SUPER|CTRL,    			-1,		XK_0,      	defaultgaps,    		{0} },
	{ ALT,              			-1,		XK_equal,      	incrgaps,       		{.i = +1 } },
	{ ALT,    				-1,		XK_minus,      	incrgaps,       		{.i = -1 } },
	{ SUPER,              			-1,		XK_equal,      	incrigaps,      		{.i = +1 } },
	{ SUPER,    				-1,		XK_minus,      	incrigaps,      		{.i = -1 } },
	{ ALT|SUPER,              		-1,		XK_equal,      	incrogaps,      		{.i = +1 } },
	{ ALT|SUPER,    			-1,		XK_minus,      	incrogaps,      		{.i = -1 } },
	{ SUPER|CTRL,              		-1,		XK_equal,      	incrihgaps,     		{.i = +1 } },
	{ SUPER|CTRL,    			-1,		XK_minus,      	incrihgaps,     		{.i = -1 } },
	{ ALT|CTRL,              		-1,		XK_equal,      	incrivgaps,     		{.i = +1 } },
	{ ALT|CTRL,    				-1,		XK_minus,      	incrivgaps,     		{.i = -1 } },
	{ ALT|SHIFT,              		-1,		XK_equal,      	incrohgaps,     		{.i = +1 } },
	{ ALT|SHIFT,    			-1,		XK_minus,      	incrohgaps,     		{.i = -1 } },
	{ SUPER|SHIFT,              		-1,		XK_equal,      	incrovgaps,     		{.i = +1 } },
	{ SUPER|SHIFT,    			-1,		XK_minus,      	incrovgaps,     		{.i = -1 } },
	{ ALT|SUPER|CTRL,			-1,		XK_n,      	shiftswaptags,  		{ .i = -1 }	},
	{ ALT|SUPER|CTRL,			-1,		XK_i,      	shiftswaptags,  		{ .i = +1 }	},
	{ SUPER|CTRL,                       	-1,		XK_n, 		shifttag,    			{ .i = -1 } },
	{ SUPER|CTRL,                       	-1,		XK_i, 		shifttag,    			{ .i = +1 } },
    	{ ALT|SUPER,           			-1,		XK_i,  		switchtags,      		{ .ui = SWITCHTAG_RIGHT  | SWITCHTAG_VIEW } },
    	{ ALT|SUPER,           			-1,		XK_n,   	switchtags,      		{ .ui = SWITCHTAG_LEFT   | SWITCHTAG_VIEW } },
	{ ALT|CTRL|SHIFT,			-1,		XK_n,      	shiftboth,      		{ .i = -1 }	},
	{ ALT|CTRL|SHIFT,             		-1,		XK_i,      	shiftboth,      		{ .i = +1 }	},
	{ ALT|CTRL,				-1,           	XK_a, 		riospawn,       		{.v = termcmd } },
	{ ALT|CTRL,				-1,             XK_r,      	rioresize,      		{0} },
	TAGKEYS(                        -1,XK_1,                      0)
	TAGKEYS(                        -1,XK_2,                      1)
	TAGKEYS(                        -1,XK_3,                      2)
	TAGKEYS(                        -1,XK_4,                      3)
	TAGKEYS(                        -1,XK_5,                      4)
	TAGKEYS(                        -1,XK_6,                      5)
	TAGKEYS(                        -1,XK_7,                      6)
	TAGKEYS(                        -1,XK_8,                      7)
	TAGKEYS(                        -1,XK_9,                      8)
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
	{ ClkClientWin,         ALT,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         ALT,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         ALT,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         SUPER, Button3,      dragcfact,      {0} },
	{ ClkClientWin,         SUPER|SHIFT, Button3,      dragmfact,      {0} },
	{ ClkClientWin,         ALT|SHIFT, Button1,      swalmouse,      {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            ALT,         Button1,        tag,            {0} },
	{ ClkTagBar,            ALT,         Button3,        toggletag,      {0} },
	{ ClkClientWin,         ALT,         Button4,        resizemousescroll, {.v = &scrollargs[0]} },
	{ ClkClientWin,         ALT,         Button5,        resizemousescroll, {.v = &scrollargs[1]} },
	{ ClkClientWin,         ALT,         Button6,        resizemousescroll, {.v = &scrollargs[2]} },
	{ ClkClientWin,         ALT,         Button7,        resizemousescroll, {.v = &scrollargs[3]} },
	{ ClkTabBar,            0,              Button1,        focuswin,       {0} },
	{ ClkTabClose,          0,              Button1,        killclient,     {0} },
	{ ClkTabNext,           0,              Button1,        focusstackvis,  {.i=+1} },
	{ ClkTabPrev,           0,              Button1,        focusstackvis,  {.i=-1} },
	{ ClkTabEmpty,          0,              Button1,        togglebar,      {0} },
	/* placemouse options, choose which feels more natural:
	 *    0 - tiled position is relative to mouse cursor
	 *    1 - tiled postiion is relative to window center
	 *    2 - mouse pointer warps to window center
	 *
	 * The moveorplace uses movemouse or placemouse depending on the floating state
	 * of the selected client. Set up individual keybindings for the two if you want
	 * to control these separately (i.e. to retain the feature to move a tiled window
	 * into a floating position).
	 */
	{ ClkClientWin,         SUPER,         Button1,        moveorplace,    {.i = 1} },
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
  IPCCOMMAND(  focusstackvis,       1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  zoom,                1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  incnmaster,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  killclient,          1,      {ARG_TYPE_SINT}   ),
  IPCCOMMAND(  togglefloating,      1,      {ARG_TYPE_NONE}   ),
  IPCCOMMAND(  setmfact,            1,      {ARG_TYPE_FLOAT}  ),
  IPCCOMMAND(  setlayoutsafe,       1,      {ARG_TYPE_PTR}    ),
  IPCCOMMAND(  quit,                1,      {ARG_TYPE_NONE}   )
};

