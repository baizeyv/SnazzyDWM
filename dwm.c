/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <ctype.h> /* for tolower function, very tiny standard library */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <Imlib2.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>
#ifdef __OpenBSD__
#include <sys/sysctl.h>
#include <kvm.h>
#endif /* __OpenBSD */

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                               * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define INTERSECTC(x,y,w,h,z)   (MAX(0, MIN((x)+(w),(z)->x+(z)->w) - MAX((x),(z)->x)) \
                               * MAX(0, MIN((y)+(h),(z)->y+(z)->h) - MAX((y),(z)->y)))
#define ISVISIBLEONTAG(C, T)    ((C->tags & T))
//#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define ISVISIBLE(C)            ISVISIBLEONTAG(C, (C->mon->tagset[C->mon->seltags]) || C->issticky)
#define HIDDEN(C)               ((getstate(C->win) == IconicState))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)
#define TEXTW2(X)                (drw_fontset_getwidth(drw, (X)))

#define SYSTEM_TRAY_REQUEST_DOCK    0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

#define OPAQUE                  0xffU

/* Undefined in X11/X.h buttons that are actualy exist and correspond to
 * horizontal scroll
 */
#define Button6			6
#define Button7			7

#define MWM_HINTS_FLAGS_FIELD       0
#define MWM_HINTS_DECORATIONS_FIELD 2
#define MWM_HINTS_DECORATIONS       (1 << 1)
#define MWM_DECOR_ALL               (1 << 0)
#define MWM_DECOR_BORDER            (1 << 1)
#define MWM_DECOR_TITLE             (1 << 3)

/* enums */
enum { CurNormal, CurResize, CurMove, CurSwal, CurResizeHorzArrow, CurResizeVertArrow, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeHid, SchemeClose, SchemePrev, SchemeNext, SchemeAttach  }; /* color schemes */
enum { NetSupported, NetWMName, NetWMIcon, NetWMState, NetWMCheck,
       NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
       NetWMFullscreen, NetActiveWindow, NetWMWindowType,
       NetWMWindowTypeDialog, NetClientList, NetWMWindowsOpacity, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMWindowRole, WMLast }; /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkTopTitle, ClkTabBar, ClkTabClose, ClkTabNext, ClkTabPrev, ClkTabEmpty,
       ClkClientWin, ClkRootWin, ClkLast }; /* clicks */
enum { ClientRegular = 1, ClientSwallowee, ClientSwallower }; /* client types */

typedef struct TagState TagState;
struct TagState {
	int selected;
	int occupied;
	int urgent;
};

typedef struct ClientState ClientState;
struct ClientState {
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky, iscentered, isalwaysontop, canfocus, cantfocus;
};

typedef union {
	long i;
	unsigned long ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	float cfact;
	int x, y, w, h;
	int sfx, sfy, sfw, sfh; /* stored float geometry, used on mode revert */
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	unsigned int tags;
	int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky, iscentered, isalwaysontop, canfocus, cantfocus;
	pid_t pid;
	int beingmoved;
	int floatborderpx;
	int hasfloatbw;
	char scratchkey;
	unsigned int switchtag;
	unsigned int icw, ich; Picture icon;
	Client *next;
	Client *snext;
	Client *swallowedby;
	Monitor *mon;
	Window win;
	ClientState prevstate;
};

typedef struct {
	unsigned int mod;
    KeySym chain;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	unsigned int signum;
	void (*func)(const Arg *);
	const Arg arg;
} Signal;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct {
	const char *class;
	const char *role;
	const char *instance;
	const char *title;
	unsigned int tags;
	int isfloating;
	int monitor;
	const char scratchkey;
    	int canfocus;
	int floatx, floaty, floatw, floath;
	int floatborderpx;
	int unmanaged;
	int switchtag;
	int iscentered;
} Rule;

typedef struct Systray   Systray;
struct Systray {
	Window win;
	Client *icons;
};

typedef struct Swallow Swallow;
struct Swallow {
	/* Window class name, instance name (WM_CLASS) and title
	 * (WM_NAME/_NET_WM_NAME, latter preferred if it exists). An empty string
	 * implies a wildcard as per strstr(). */
	char class[256];
	char inst[256];
	char title[256];

	/* Used to delete swallow instance after 'swaldecay' windows were mapped
	 * without the swallow having been consumed. 'decay' keeps track of the
	 * remaining "charges". */
	int decay;

	/* The swallower, i.e. the client which will swallow the next mapped window
	 * whose filters match the above properties. */
	Client *client;

	/* Linked list of registered swallow instances. */
	Swallow *next;
};

typedef struct {
	int monitor;
	int layout;
	float mfact;
	int nmaster;
	int showbar;
	int topbar;
} MonitorRule;

/* Xresources preferences */
enum resource_type {
	STRING = 0,
	INTEGER = 1,
	FLOAT = 2
};

typedef struct {
	char *name;
	enum resource_type type;
	void *dst;
} ResourcePref;

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void aspectresize(const Arg *arg);
static void attach(Client *c);
static void attachstack(Client *c);
static void attachx(Client *c);
static void toggleattachx();
static void toggleattachxstr();
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void cyclelayout(const Arg *arg);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void dragmfact(const Arg *arg);
static void dragcfact(const Arg *arg);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawtab(Monitor *m);
static void drawtabs(void);
static int cmpint(const void *p1, const void *p2);
static void drawtaggrid(Monitor *m, int *x_pos, unsigned int occ);
static void distributetags(const Arg *arg);
static int drawstatusbar(Monitor *m, int bh, char* text, int extra);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static Client *findbefore(Client *c);
static int fakesignal(void);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmaster(const Arg *arg);
static void focusmon(const Arg *arg);
static void focusstackvis(const Arg *arg);
static void focusstackhid(const Arg *arg);
static void focusstack(int inc, int vis);
static pid_t getparentprocess(pid_t p);
static void focuswin(const Arg* arg);
static Picture geticonprop(Window w, unsigned int *icw, unsigned int *ich);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static pid_t getstatusbarpid();
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void goback(const Arg *arg);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int handlexevent(struct epoll_event *ev);
static void hide(const Arg *arg);
static void hidewin(Client *c);
static void incnmaster(const Arg *arg);
static int isdescprocess(pid_t p, pid_t c);
static void inplacerotate(const Arg *arg);
static void keypress(XEvent *e);
static int fake_signal(void);
static void keyrelease(XEvent *e);
static void killclient(const Arg *arg);
static void layoutmenu(const Arg *arg);
static void killunsel(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void moveresize(const Arg *arg);
static void moveresizeedge(const Arg *arg);
static void movemouse(const Arg *arg);
static void moveorplace(const Arg *arg);
static Client *nexttiled(Client *c);
static void placemouse(const Arg *arg);
static void opacity(Client *c, double opacity);
//static void pop(Client *);
static Client *prevtiled(Client *c);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static Client *recttoclient(int x, int y, int w, int h);
static Monitor *recttomon(int x, int y, int w, int h);
static void reorganizetags(const Arg *arg);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resetcanfocusfloating();
static void resizemousescroll(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static int riodraw(Client *c, const char slopstyle[]);
static void rioposition(Client *c, int x, int y, int w, int h);
static void rioresize(const Arg *arg);
static void riospawn(const Arg *arg);
static void run(void);
static void runautostart(void);
static void scan(void);
static void scratchpad_hide ();
static _Bool scratchpad_last_showed_is_killed (void);
static void scratchpad_remove ();
static void scratchpad_show ();
static void scratchpad_show_client (Client * c);
static void scratchpad_show_first (void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendmon(Client *c, Monitor *m);
static void setborderpx(const Arg *arg);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setlayoutsafe(const Arg *arg);
static void setmark(Client *c);
static void setcfact(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void setupepoll(void);
static void seturgent(Client *c, int urg);
static void show(const Arg *arg);
static void showwin(Client *c);
static void showhide(Client *c);
static void showtagpreview(int tag);
static void sigchld(int unused);
static void sigstatusbar(const Arg *arg);
static void sighup(int unused);
static void sigterm(int unused);
static void spawn(const Arg *arg);
static pid_t spawncmd(const Arg *arg);
static void tabmode(const Arg *arg);
static void switchtags(const Arg *arg);
static void swapclient(const Arg *arg);
static void swapfocus(const Arg *arg);
static void spawnscratch(const Arg *arg);
static void swal(Client *swer, Client *swee, int manage);
static void swalreg(Client *c, const char* class, const char* inst, const char* title);
static void swaldecayby(int decayby);
static void swalmanage(Swallow *s, Window w, XWindowAttributes *wa);
static Swallow *swalmatch(Window w);
static void swalmouse(const Arg *arg);
static void swalrm(Swallow *s);
static void swalunreg(Client *c);
static void swalstop(Client *c, Client *root);
static void swalstopsel(const Arg *unused);
static void switchtag(void);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagto(const Arg *arg);
static void tagmon(const Arg *arg);
static void togglealttag();
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglefullscr(const Arg *arg);
static void togglecanfocusfloating(const Arg *arg);
static void togglefloatcenter(const Arg *arg);
static void togglemark(const Arg *arg);
static void togglescratch(const Arg *arg);
static void togglealwaysontop(const Arg *arg);
static void togglesticky(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void freeicon(Client *c);
static void togglewin(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatemotifhints(Client *c);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updateicon(Client *c);
static void updatepreview(void);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static pid_t winpid(Window w);
static Client *wintoclient(Window w);
static int wintoclient2(Window w, Client **pc, Client **proot);
static Monitor *wintomon(Window w);
static void winview(const Arg* arg);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void xinitvisual();
static void zoom(const Arg *arg);
static void load_xresources(void);
static void resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst);

static void shifttag(const Arg *arg);
static void shifttagclients(const Arg *arg);
static void shiftview(const Arg *arg);
static void shiftviewclients(const Arg *arg);
static void shiftboth(const Arg *arg);
static void shiftswaptags(const Arg *arg);
static void swaptags(const Arg *arg);

/* Key binding functions */
static void defaultgaps(const Arg *arg);
static void incrgaps(const Arg *arg);
static void incrigaps(const Arg *arg);
static void incrogaps(const Arg *arg);
static void incrohgaps(const Arg *arg);
static void incrovgaps(const Arg *arg);
static void incrihgaps(const Arg *arg);
static void incrivgaps(const Arg *arg);
static void togglegaps(const Arg *arg);
/* Layouts (delete the ones you do not need) */
static void bstack(Monitor *m);
static void bstackhoriz(Monitor *m);
static void centeredmaster(Monitor *m);
static void centeredfloatingmaster(Monitor *m);
static void deck(Monitor *m);
static void dwindle(Monitor *m);
static void fibonacci(Monitor *m, int s);
static void grid(Monitor *m);
static void horizgrid(Monitor *m);
static void gaplessgrid(Monitor *m);
static void nrowgrid(Monitor *m);
static void spiral(Monitor *m);
static void tile(Monitor *m);
/* Internals */
static void getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc);
static void getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr, int *sr);
static void setgaps(int oh, int ov, int ih, int iv);

/* Settings */
#if !PERTAG_PATCH
static int enablegaps = 1;
#endif // PERTAG_PATCH

static void keyrelease2(XEvent *e);
static void combotag(const Arg *arg);
static void comboview(const Arg *arg);


/* variables */
static int aftertabx = 0, viewwidth = 0;
static int newx = 0, newy = 0, tmpx = 0, tmpy = 0;
static int swflag = 0;
static Client *prevzoom = NULL;
static Systray *systray =  NULL;
static const char autostartblocksh[] = "autostart_blocking.sh";
static const char autostartsh[] = "autostart.sh";
static const char broken[] = "broken";
static const char dwmdir[] = "dwm";
static const char localshare[] = ".local/share";
static char stext[1024];
static char estext[1024];
static int statussig;
static int statusw;
static int statusew;
static pid_t statuspid = -1;
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int th = 0;           /* tab bar geometry */
static int unmanaged = 0;    /* whether the window manager should manage the new window or not */
static int lrpad;            /* sum of left and right padding for text */
static int vp;               /* vertical padding for bar */
static int sp;               /* side padding for bar */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static int riodimensions[4] = { -1, -1, -1, -1 };
static pid_t riopid = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ButtonRelease] = keyrelease,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyRelease] = keyrelease2,
	[KeyPress] = keypress,
    	[KeyRelease] = keyrelease,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[MotionNotify] = motionnotify,
	[PropertyNotify] = propertynotify,
	[ResizeRequest] = resizerequest,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast], motifatom;
static int epoll_fd;
static int dpy_fd;
static int restart = 0;
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Clr **tagscheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon, *lastselmon, *prevmon;
static Swallow *swallows;
static Window root, wmcheckwin;
static Client *mark;
static KeySym keychain = -1;
static xcb_connection_t *xcon;

static int useargb = 0;
static Visual *visual;
static int depth;
static Colormap cmap;

/* scratchpad */
# define SCRATCHPAD_MASK (1u << sizeof tags / sizeof * tags)
static Client * scratchpad_last_showed = NULL;

#include "ipc.h"

/* configuration, allows nested code to access above variables */
#include "config.h"

#ifdef VERSION
#include "IPCClient.c"
#endif

#include <fcntl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_tree.h>

#include "yajl_dumps.h"

unsigned int tagw[LENGTH(tags)];
unsigned int alttagw[LENGTH(tags)];

static struct sockaddr_un sockaddr;
static struct epoll_event sock_epoll_event;
static IPCClientList ipc_clients = NULL;
static int epoll_fd = -1;
static int sock_fd = -1;
static IPCCommand *ipc_commands;
static unsigned int ipc_commands_len;
// Max size is 1 MB
static const uint32_t MAX_MESSAGE_SIZE = 1000000;
static const int IPC_SOCKET_BACKLOG = 5;

#define MAXTABS 50

typedef struct Pertag Pertag;
struct Monitor {
	char ltsymbol[16];
	char lastltsymbol[16];
	float mfact;
	int nmaster;
	int num;
	int by;               /* bar geometry */
	int ty;               /* tab bar geometry */
	int btw;              /* width of tasks portion of bar */
	int bt;               /* number of tasks */
	int eby;              /* extra bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	unsigned int borderpx;
	int gappih;           /* horizontal gap between windows */
	int gappiv;           /* vertical gap between windows */
	int gappoh;           /* horizontal outer gaps */
	int gappov;           /* vertical outer gaps */
	unsigned int seltags;
	unsigned int sellt;
	unsigned int tagset[2];
	TagState tagstate;
	int showbar;
	int showtab;
	int topbar;
	int toptab;
	Window tabwin;
	int ntabs;
	int tab_widths[MAXTABS];
	int hidsel;
	Client *clients;
	Client *sel;
	Client *lastsel;
	Client *stack;
	Client *tagmarked[32];
	Monitor *next;
	Window barwin;
	Window extrabarwin;
	const Layout *lt[2];
	const Layout *lastlt;
	Pertag *pertag;
	unsigned int alttag;
	Window tagwin;
	int previewshow;
	Pixmap tagmap[LENGTH(tags)];
};

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
	int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 30 ? -1 : 1]; };

/* function implementations */
static int combo = 0;

void
keyrelease2(XEvent *e) {
	combo = 0;
}

void
combotag(const Arg *arg) {
	if(selmon->sel && arg->ui & TAGMASK) {
		if (combo) {
			selmon->sel->tags |= arg->ui & TAGMASK;
		} else {
			combo = 1;
			selmon->sel->tags = arg->ui & TAGMASK;
		}
		focus(NULL);
		arrange(selmon);
	}
}

void
comboview(const Arg *arg) {
	unsigned newtags = arg->ui & TAGMASK;
	if (combo) {
		selmon->tagset[selmon->seltags] |= newtags;
	} else {
		selmon->seltags ^= 1;	/*toggle tagset*/
		combo = 1;
		if (newtags)
			selmon->tagset[selmon->seltags] = newtags;
	}
	focus(NULL);
	arrange(selmon);
}

void
applyrules(Client *c)
{
	const char *class, *instance;
	char role[64];
	unsigned int i, newtagset;
	const Rule *r;
	Monitor *m;
	XClassHint ch = { NULL, NULL };

	/* rule matching */
	c->iscentered = 0;
	c->isfloating = 0;
	c->tags = 0;
	c->scratchkey = 0;
    	c->canfocus = 1;
	XGetClassHint(dpy, c->win, &ch);
	class    = ch.res_class ? ch.res_class : broken;
	instance = ch.res_name  ? ch.res_name  : broken;
	gettextprop(c->win, wmatom[WMWindowRole], role, sizeof(role));

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((!r->title || strstr(c->name, r->title))
		&& (!r->class || strstr(class, r->class))
		&& (!r->role || strstr(role, r->role))
		&& (!r->instance || strstr(instance, r->instance)))
		{
			c->iscentered = r->iscentered;
			c->isfloating = r->isfloating;
			c->tags |= r->tags;
			c->scratchkey = r->scratchkey;
            		c->canfocus = r->canfocus;
			if (r->floatborderpx >= 0) {
				c->floatborderpx = r->floatborderpx;
				c->hasfloatbw = 1;
			}
			if (r->isfloating) {
				if (r->floatx >= 0) c->x = c->mon->mx + r->floatx;
				if (r->floaty >= 0) c->y = c->mon->my + r->floaty;
				if (r->floatw >= 0) c->w = r->floatw;
				if (r->floath >= 0) c->h = r->floath;
			}
			unmanaged = r->unmanaged;
			for (m = mons; m && m->num != r->monitor; m = m->next);
			if (m)
				c->mon = m;

			if (r->switchtag) {
				selmon = c->mon;
				if (r->switchtag == 2 || r->switchtag == 4)
					newtagset = c->mon->tagset[c->mon->seltags] ^ c->tags;
				else
					newtagset = c->tags;

				if (newtagset && !(c->tags & c->mon->tagset[c->mon->seltags])) {
					if (r->switchtag == 3 || r->switchtag == 4)
						c->switchtag = c->mon->tagset[c->mon->seltags];
					if (r->switchtag == 1 || r->switchtag == 3)
						view(&((Arg) { .ui = newtagset }));
					else {
						c->mon->tagset[c->mon->seltags] = newtagset;
						arrange(c->mon);
					}
				}
			}
		}
	}
	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	if (c->tags != SCRATCHPAD_MASK) {
		if(c->tags & TAGMASK)                    c->tags = c->tags & TAGMASK;
		else if(c->mon->tagset[c->mon->seltags]) c->tags = c->mon->tagset[c->mon->seltags];
		else                                     c->tags = 1;
	}
}

int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
	int baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if (interact) {
		if (*x > sw)
			*x = sw - WIDTH(c);
		if (*y > sh)
			*y = sh - HEIGHT(c);
		if (*x + *w + 2 * c->bw < 0)
			*x = 0;
		if (*y + *h + 2 * c->bw < 0)
			*y = 0;
	} else {
		if (*x >= m->wx + m->ww)
			*x = m->wx + m->ww - WIDTH(c);
		if (*y >= m->wy + m->wh)
			*y = m->wy + m->wh - HEIGHT(c);
		if (*x + *w + 2 * c->bw <= m->wx)
			*x = m->wx;
		if (*y + *h + 2 * c->bw <= m->wy)
			*y = m->wy;
	}
	if (*h < bh)
		*h = bh;
	if (*w < bh)
		*w = bh;
	if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if (!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if (c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if (baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if (c->incw)
			*w -= *w % c->incw;
		if (c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w = MAX(*w + c->basew, c->minw);
		*h = MAX(*h + c->baseh, c->minh);
		if (c->maxw)
			*w = MIN(*w, c->maxw);
		if (c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange(Monitor *m)
{
	if (m)
		showhide(m->stack);
	else for (m = mons; m; m = m->next)
		showhide(m->stack);
	if (m) {
		arrangemon(m);
		restack(m);
	} else for (m = mons; m; m = m->next)
		arrangemon(m);
}

void
arrangemon(Monitor *m) {
	updatebarpos(m);
	XMoveResizeWindow(dpy, m->tabwin, m->wx + sp, m->ty, m->ww - 2 * sp, th);
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
}

void
attachx(Client *c)
{
	Client *at;
	unsigned int n;

	switch (attachmode) {
		case 1: // above
			if (c->mon->sel == NULL || c->mon->sel == c->mon->clients || c->mon->sel->isfloating)
				break;

			for (at = c->mon->clients; at->next != c->mon->sel; at = at->next);
			c->next = at->next;
			at->next = c;
			return;

		case 2: // aside
			for (at = c->mon->clients, n = 0; at; at = at->next)
				if (!at->isfloating && ISVISIBLEONTAG(at, c->tags))
					if (++n >= c->mon->nmaster)
						break;

			if (!at || !c->mon->nmaster)
				break;

			c->next = at->next;
			at->next = c;
			return;

		case 3: // below
			if (c->mon->sel == NULL || c->mon->sel->isfloating)
				break;

			c->next = c->mon->sel->next;
			c->mon->sel->next = c;
			return;

		case 4: // bottom
			for (at = c->mon->clients; at && at->next; at = at->next);
			if (!at)
				break;

			at->next = c;
			c->next = NULL;
			return;
	}

	/* master (default) */
	attach(c);
}

void
toggleattachx()
{
	attachmode = (attachmode + 1) % 4;
	drawbars();
	drawtabs();
}

void
toggleattachxstr()
{
	showattachmodestr = !showattachmodestr;
}

void
aspectresize(const Arg *arg) {
	/* only floating windows can be moved */
	Client *c;
	c = selmon->sel;
	float ratio;
	int w, h,nw, nh;

	if (!c || !arg)
		return;
	if (selmon->lt[selmon->sellt]->arrange && !c->isfloating)
		return;

	ratio = (float)c->w / (float)c->h;
	h = arg->i;
	w = (int)(ratio * h);

	nw = c->w + w;
	nh = c->h + h;

	XRaiseWindow(dpy, c->win);
	resize(c, c->x, c->y, nw, nh, True);
}

void
attach(Client *c)
{
	c->next = c->mon->clients;
	c->mon->clients = c;
}

void
attachstack(Client *c)
{
	c->snext = c->mon->stack;
	c->mon->stack = c;
}

void
buttonpress(XEvent *e)
{
	unsigned int i, x, click, occ = 0;
	unsigned int columns;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	columns = LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);
	click = ClkRootWin;
	/* focus monitor if necessary */
	if ((m = wintomon(ev->window)) && m != selmon) {
		unfocus(selmon->sel, 1);
		prevmon = selmon;
		selmon = m;
		focus(NULL);
	}
	if (ev->window == selmon->extrabarwin) {
		if(extrabarright) {
			if (ev->x >= selmon->ww - statusew - 2 * sp) {
				x = selmon->ww - statusew - 2 * sp + horizpadbar / 2;
				click = ClkStatusText;
				char *text, *s, ch;
				statussig = 0;
				for (text = s = estext; *s && x <= ev->x; s++) {
					if ((unsigned char)(*s) < ' ') {
						ch = *s;
						*s = '\0';
						x += TEXTW(text) - lrpad;
						*s = ch;
						text = s + 1;
						if (x >= ev->x)
							break;
						statussig = ch;
					} else if (*s == '^') {
						*s = '\0';
						x += TEXTW(text) - lrpad;
						*s = '^';
						if (*(++s) == 'f')
							x += atoi(++s);
						while (*(s++) != '^');
						text = s;
						s--;
					}
				}
			} else {
				x = 0;
				c = m->clients;

				if (c) {
					do {
						if (!ISVISIBLE(c))
							continue;
						else
							x += (1.0 / (double)m->bt) * (m->ww - statusew - 2 * sp);
					} while (ev->x > x && (c = c->next));

					click = ClkWinTitle;
					arg.v = c;
				}
			}
		} else {
			if (ev->x >= 0 && ev->x <= statusew) {
				x = horizpadbar / 2;
				click = ClkStatusText;
				char *text, *s, ch;
				statussig = 0;
				for (text = s = estext; *s && x <= ev->x; s++) {
					if ((unsigned char)(*s) < ' ') {
						ch = *s;
						*s = '\0';
						x += TEXTW(text) - lrpad;
						*s = ch;
						text = s + 1;
						if (x >= ev->x)
							break;
						statussig = ch;
					} else if (*s == '^') {
						*s = '\0';
						x += TEXTW(text) - lrpad;
						*s = '^';
						if (*(++s) == 'f')
							x += atoi(++s);
						while (*(s++) != '^');
						text = s;
						s--;
					}
				}
			} else {
				x = statusew;
				c = m->clients;

				if (c) {
					do {
						if (!ISVISIBLE(c))
							continue;
						else
							x += (1.0 / (double)m->bt) * (m->ww - statusew - 2 * sp);
					} while (ev->x > x && (c = c->next));

					click = ClkWinTitle;
					arg.v = c;
				}
			}
		}
	}
	if (ev->window == selmon->barwin) {
		if (selmon->previewshow) {
			XUnmapWindow(dpy, selmon->tagwin);
				selmon->previewshow = 0;
		}
		i = x = 0;
		for (c = m->clients; c; c = c->next)
			occ |= c->tags == 255 ? 0 : c->tags;
		do {
			/* do not reserve space for vacant tags */
			if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
				continue;
			x += selmon->alttag ? alttagw[i] : tagw[i];
		} while (ev->x >= x && ++i < LENGTH(tags));
		if (i < LENGTH(tags)) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		} else if (ev->x < x + blw)
			click = ClkLtSymbol;
		else if (ev->x > selmon->ww - statusw) {
			x = selmon->ww - statusw;
			click = ClkStatusText;
			char *text, *s, ch;
			statussig = 0;
			for (text = s = stext; *s && x <= ev->x; s++) {
				if ((unsigned char)(*s) < ' ') {
					ch = *s;
					*s = '\0';
					x += TEXTW(text) - lrpad;
					*s = ch;
					text = s + 1;
					if (x >= ev->x)
						break;
					statussig = ch;
				} else if (*s == '^') {
					*s = '\0';
					x += TEXTW(text) - lrpad;
					*s = '^';
					if (*(++s) == 'f')
						x += atoi(++s);
					while (*(s++) != '^');
					text = s;
					s--;
				}
			}
		} else if (ev->x >= selmon->ww - statusw - columns * bh / tagrows + 1 && (drawtagmask & DRAWTAGGRID)) {
			x = selmon->ww - statusw - columns * bh / tagrows + 1;
			click = ClkTagBar;
			i = (ev->x - x) / (bh / tagrows);
			i = i + columns * (ev->y / (bh / tagrows));
			if (i >= LENGTH(tags)) {
				i = LENGTH(tags) - 1;
			}
			arg.ui = 1 << i;
		} else
			click = ClkTopTitle;
	}
	if(ev->window == selmon->tabwin) {
		i = 0; x = horizpadbar / 2;
		for(c = selmon->clients; c; c = c->next){
			if(!ISVISIBLE(c)) continue;
			x += selmon->tab_widths[i];
			if (ev->x > x)
				++i;
			else
				break;
			if(i >= m->ntabs) break;
		}
		if(c && ev->x <= x && ev->x >= horizpadbar / 2) {
			click = ClkTabBar;
			arg.ui = i;
		} else if(c && ev->x <= horizpadbar / 2) {
		} else if(c && ev->x <= aftertabx - horizpadbar / 2 && ev->x >= x) {
			click = ClkTabEmpty;
		} else if(c && ev->x <= selmon->ww - horizpadbar - 2 - TEXTW2(btn_close) - TEXTW2(btn_next) - 2 * horizpadbar - TEXTW2(btn_prev) - horizpadbar && ev->x >= aftertabx - 2 - horizpadbar / 2) {
			click = ClkLtSymbol;
		} else if(c && ev->x <= selmon->ww - horizpadbar - 2 - TEXTW2(btn_close) - TEXTW2(btn_next) - 2 * horizpadbar) {
			click = ClkTabPrev;
		} else if(c && ev->x <= selmon->ww - horizpadbar - 2 - TEXTW2(btn_close) - horizpadbar) {
			click = ClkTabNext;
		} else if(c && ev->x <= selmon->ww - horizpadbar - 2) {
			click = ClkTabClose;
		}
	}
	else if((c = wintoclient(ev->window))) {
		focus(c);
		restack(selmon);
		XAllowEvents(dpy, ReplayPointer, CurrentTime);
		click = ClkClientWin;
	}
	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
		&& CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
			buttons[i].func((click == ClkTagBar || click == ClkWinTitle || click == ClkTabBar) && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	Arg a = {.ui = ~0};
	Layout foo = { "", NULL };
	Monitor *m;
	size_t i;

	view(&a);
	selmon->lt[selmon->sellt] = &foo;
	for (m = mons; m; m = m->next)
		while (m->stack)
			unmanage(m->stack, 0);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	while (mons)
		cleanupmon(mons);
	if (showsystray) {
		XUnmapWindow(dpy, systray->win);
		XDestroyWindow(dpy, systray->win);
		free(systray);
	}
	for (i = 0; i < CurLast; i++)
		drw_cur_free(drw, cursor[i]);
	for (i = 0; i < LENGTH(colors) + 1; i++)
		free(scheme[i]);
	XDestroyWindow(dpy, wmcheckwin);
	drw_free(drw);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	XDeleteProperty(dpy, root, netatom[NetActiveWindow]);

	ipc_cleanup();

	if (close(epoll_fd) < 0) {
			fprintf(stderr, "Failed to close epoll file descriptor\n");
	}
}

void
cleanupmon(Monitor *mon)
{
	Monitor *m;

	if (mon == mons)
		mons = mons->next;
	else {
		for (m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	for (size_t i = 0; i < LENGTH(tags); i++)
		if (mon->tagmap[i])
			XFreePixmap(dpy, mon->tagmap[i]);
	XUnmapWindow(dpy, mon->barwin);
	XUnmapWindow(dpy, mon->extrabarwin);
	XDestroyWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->extrabarwin);
	XUnmapWindow(dpy, mon->tabwin);
	XDestroyWindow(dpy, mon->tabwin);
	XUnmapWindow(dpy, mon->tagwin);
	XDestroyWindow(dpy, mon->tagwin);
	free(mon);
}

void
clientmessage(XEvent *e)
{
	XWindowAttributes wa;
	XSetWindowAttributes swa;
	XClientMessageEvent *cme = &e->xclient;
	Client *c = wintoclient(cme->window);

	if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
		/* add systray icons */
		if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
			if (!(c = (Client *)calloc(1, sizeof(Client))))
				die("fatal: could not malloc() %u bytes\n", sizeof(Client));
			if (!(c->win = cme->data.l[2])) {
				free(c);
				return;
			}
			c->mon = selmon;
			c->next = systray->icons;
			systray->icons = c;
			if (!XGetWindowAttributes(dpy, c->win, &wa)) {
				/* use sane defaults */
				wa.width = bh;
				wa.height = bh;
				wa.border_width = 0;
			}
			c->x = c->oldx = c->y = c->oldy = 0;
			c->w = c->oldw = wa.width;
			c->h = c->oldh = wa.height;
			c->oldbw = wa.border_width;
			c->bw = 0;
			c->isfloating = True;
			/* reuse tags field as mapped status */
			c->tags = 1;
			updatesizehints(c);
			updatesystrayicongeom(c, wa.width, wa.height);
			XAddToSaveSet(dpy, c->win);
			XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
			XReparentWindow(dpy, c->win, systray->win, 0, 0);
			/* use parents background color */
			swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
			XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			/* FIXME not sure if I have to send these events, too */
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
			XSync(dpy, False);
			resizebarwin(selmon);
			updatesystray();
			setclientstate(c, NormalState);
		}
		return;
	}
	if (!c)
		return;
	if (cme->message_type == netatom[NetWMState]) {
		if (cme->data.l[1] == netatom[NetWMFullscreen]
		|| cme->data.l[2] == netatom[NetWMFullscreen])
			setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
				|| (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
	} else if (cme->message_type == netatom[NetActiveWindow]) {
		if (c != selmon->sel && !c->isurgent)
			seturgent(c, 1);
	}
}

void
configure(Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e)
{
	Monitor *m;
	Client *c;
	XConfigureEvent *ev = &e->xconfigure;
	int dirty;

	/* TODO: updategeom handling sucks, needs to be simplified */
	if (ev->window == root) {
		dirty = (sw != ev->width || sh != ev->height);
		sw = ev->width;
		sh = ev->height;
		if (updategeom() || dirty) {
			drw_resize(drw, sw, bh);
			updatebars();
			for (m = mons; m; m = m->next) {
				for (c = m->clients; c; c = c->next)
					if (c->isfullscreen)
						resizeclient(c, m->mx, m->my, m->mw, m->mh);
				resizebarwin(m);
				XMoveResizeWindow(dpy, m->extrabarwin, m->wx + sp, m->eby - vp, m->ww - 2 * sp, bh);
			}
			focus(NULL);
			arrange(NULL);
		}
	}
}

void
configurerequest(XEvent *e)
{
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	switch (wintoclient2(ev->window, &c, NULL)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		if (ev->value_mask & CWBorderWidth) {
			c->bw = ev->border_width;
		} else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
			m = c->mon;
			if (ev->value_mask & CWX) {
				c->oldx = c->x;
				c->x = m->mx + ev->x;
			}
			if (ev->value_mask & CWY) {
				c->oldy = c->y;
				c->y = m->my + ev->y;
			}
			if (ev->value_mask & CWWidth) {
				c->oldw = c->w;
				c->w = ev->width;
			}
			if (ev->value_mask & CWHeight) {
				c->oldh = c->h;
				c->h = ev->height;
			}
			if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
				c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
			if ((c->y + c->h) > m->my + m->mh && c->isfloating)
				c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
			if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
				configure(c);
			if (ISVISIBLE(c))
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		} else
			configure(c);
		break;
	case ClientSwallower:
		/* Reject any move/resize requests for swallowers and communicate
		 * refusal to client via a synthetic ConfigureNotify (ICCCM 4.1.5). */
		configure(c);
		break;
	default:
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
		break;
	}
	XSync(dpy, False);
}

Monitor *
createmon(void)
{
	unsigned int i;
	Monitor *m, *mon;
	unsigned int mi, j;
	const MonitorRule *mr;

	m = ecalloc(1, sizeof(Monitor));
	m->tagset[0] = m->tagset[1] = startontag ? 1 : 0;
	m->mfact = mfact;
	m->nmaster = nmaster;
	m->showbar = showbar;
	m->showtab = showtab;
	m->topbar = topbar;
	m->toptab = toptab;
	m->ntabs = 0;
	m->borderpx = borderpx;
	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	for (mi = 0, mon = mons; mon; mon = mon->next, mi++);
	for (j = 0; j < LENGTH(monrules); j++) {
		mr = &monrules[j];
		if ((mr->monitor == -1 || mr->monitor == mi)) {
			m->lt[0] = &layouts[mr->layout];
			m->lt[1] = &layouts[1 % LENGTH(layouts)];
			strncpy(m->ltsymbol, layouts[mr->layout].symbol, sizeof m->ltsymbol);

			if (mr->mfact > -1)
				m->mfact = mr->mfact;
			if (mr->nmaster > -1)
				m->nmaster = mr->nmaster;
			if (mr->showbar > -1)
				m->showbar = mr->showbar;
			if (mr->topbar > -1)
				m->topbar = mr->topbar;
			break;
		}
	}
	m->pertag = ecalloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;

		m->pertag->showbars[i] = m->showbar;
	}

	return m;
}

void
cyclelayout(const Arg *arg) {
	Layout *l;
	for(l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++);
	if(arg->i > 0) {
		if(l->symbol && (l + 1)->symbol)
			setlayout(&((Arg) { .v = (l + 1) }));
		else
			setlayout(&((Arg) { .v = layouts }));
	} else {
		if(l != layouts && (l - 1)->symbol)
			setlayout(&((Arg) { .v = (l - 1) }));
		else
			setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
	}
}

void
destroynotify(XEvent *e)
{
	Client *c, *swee, *root;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	switch (wintoclient2(ev->window, &c, &root)) {
	case ClientRegular:
		unmanage(c, 1);
		break;
	case ClientSwallowee:
		swalstop(c, NULL);
		unmanage(c, 1);
		break;
	case ClientSwallower:
		/* If the swallower is swallowed by another client, terminate the
		 * swallow. This cuts off the swallow chain after the client. */
		swalstop(c, root);

		/* Cut off the swallow chain before the client. */
		for (swee = root; swee->swallowedby != c; swee = swee->swallowedby);
		swee->swallowedby = NULL;

		free(c);
		updateclientlist();
		break;
	}
	if ((c = wintosystrayicon(ev->window))) {
		removesystrayicon(c);
		resizebarwin(selmon);
		updatesystray();
	}
}

void
detach(Client *c)
{
	Client **tc;

	for (int i = 1; i < LENGTH(tags); i++)
		if (c == c->mon->tagmarked[i])
			c->mon->tagmarked[i] = NULL;

	for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
reorganizetags(const Arg *arg) {
	Client *c;
	unsigned int occ, unocc, i;
	unsigned int tagdest[LENGTH(tags)];

	occ = 0;
	for (c = selmon->clients; c; c = c->next)
		occ |= (1 << (ffs(c->tags)-1));
	unocc = 0;
	for (i = 0; i < LENGTH(tags); ++i) {
		while (unocc < i && (occ & (1 << unocc)))
			unocc++;
		if (occ & (1 << i)) {
			tagdest[i] = unocc;
			occ &= ~(1 << i);
			occ |= 1 << unocc;
		}
	}

	for (c = selmon->clients; c; c = c->next)
		c->tags = 1 << tagdest[ffs(c->tags)-1];
	if (selmon->sel)
		selmon->tagset[selmon->seltags] = selmon->sel->tags;
	arrange(selmon);
}

void
detachstack(Client *c)
{
	Client **tc, *t;

	for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == c->mon->sel) {
		for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
		c->mon->sel = t;
	}
}

Monitor *
dirtomon(int dir)
{
	Monitor *m = NULL;

	if (dir > 0) {
		if (!(m = selmon->next))
			m = mons;
	} else if (selmon == mons)
		for (m = mons; m->next; m = m->next);
	else
		for (m = mons; m->next != selmon; m = m->next);
	return m;
}

int
drawstatusbar(Monitor *m, int bh, char* stext, int extra) {
	int ret, i, j, w, x, len;
	short isCode = 0;
	char *text;
	char *p;

	len = strlen(stext) + 1 ;
	if (!(text = (char*) malloc(sizeof(char)*len)))
		die("malloc");
	p = text;

	i = -1, j = 0;
	while (stext[++i])
		if ((unsigned char)stext[i] >= ' ')
			text[j++] = stext[i];
	text[j] = '\0';

	/* compute width of the status text */
	w = 0;
	i = -1;
	while (text[++i]) {
		if (text[i] == '^') {
			if (!isCode) {
				isCode = 1;
				text[i] = '\0';
				w += TEXTW(text) - lrpad;
				text[i] = '^';
				if (text[++i] == 'f')
					w += atoi(text + ++i);
			} else {
				isCode = 0;
				text = text + i + 1;
				i = -1;
			}
		}
	}
	if (!isCode)
		w += TEXTW(text) - lrpad;
	else
		isCode = 0;
	text = p;

	w += horizpadbar;
	if (extra) {
		if (extrabarright) {
			ret = m->ww - w;
			x = ret - 2 * sp;
		} else {
			x = 0;
			ret = w;
		}
	} else {
		ret = x = m->ww - w - getsystraywidth() - 2 * sp;
	}

	drw_setscheme(drw, scheme[LENGTH(colors)]);
	drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
	drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
	drw_rect(drw, x, 0, w, bh, 1, 1);
	x += horizpadbar / 2;

	/* process status text */
	i = -1;
	while (text[++i]) {
		if (text[i] == '^' && !isCode) {
			isCode = 1;

			text[i] = '\0';
			w = TEXTW(text) - lrpad;
			drw_text(drw, x, vertpadbar / 2, w, bh - vertpadbar, 0, text, 0);

			x += w;

			/* process code */
			while (text[++i] != '^') {
				if (text[i] == 'c') {
					char buf[8];
					memcpy(buf, (char*)text+i+1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColFg], buf, baralpha);
					i += 7;
				} else if (text[i] == 'b') {
					char buf[8];
					memcpy(buf, (char*)text+i+1, 7);
					buf[7] = '\0';
					drw_clr_create(drw, &drw->scheme[ColBg], buf, baralpha);
					i += 7;
				} else if (text[i] == 'd') {
					drw->scheme[ColFg] = scheme[SchemeNorm][ColFg];
					drw->scheme[ColBg] = scheme[SchemeNorm][ColBg];
				} else if (text[i] == 'r') {
					int rx = atoi(text + ++i);
					while (text[++i] != ',');
					int ry = atoi(text + ++i);
					while (text[++i] != ',');
					int rw = atoi(text + ++i);
					while (text[++i] != ',');
					int rh = atoi(text + ++i);

					drw_rect(drw, rx + x, ry + vertpadbar / 2, rw, rh, 1, 0);
				} else if (text[i] == 'f') {
					x += atoi(text + ++i);
				}
			}

			text = text + i + 1;
			i=-1;
			isCode = 0;
		}
	}

	if (!isCode) {
		w = TEXTW(text) - lrpad;
		drw_text(drw, x, vertpadbar / 2, w, bh - vertpadbar, 0, text, 0);
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	free(p);

	return ret;
}

void
dragcfact(const Arg *arg)
{
	int prev_x, prev_y, dist_x, dist_y;
	float fact;
	Client *c;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfloating) {
		resizemouse(arg);
		return;
	}
	#if !FAKEFULLSCREEN_PATCH
	#if FAKEFULLSCREEN_CLIENT_PATCH
	if (c->isfullscreen && !c->fakefullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	#else
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	#endif // FAKEFULLSCREEN_CLIENT_PATCH
	#endif // !FAKEFULLSCREEN_PATCH
	restack(selmon);

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w/2, c->h/2);

	prev_x = prev_y = -999999;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;
			if (prev_x == -999999) {
				prev_x = ev.xmotion.x_root;
				prev_y = ev.xmotion.y_root;
			}

			dist_x = ev.xmotion.x - prev_x;
			dist_y = ev.xmotion.y - prev_y;

			if (abs(dist_x) > abs(dist_y)) {
				fact = (float) 4.0 * dist_x / c->mon->ww;
			} else {
				fact = (float) -4.0 * dist_y / c->mon->wh;
			}

			if (fact)
				setcfact(&((Arg) { .f = fact }));

			prev_x = ev.xmotion.x;
			prev_y = ev.xmotion.y;
			break;
		}
	} while (ev.type != ButtonRelease);


	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w/2, c->h/2);

	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
dragmfact(const Arg *arg)
{
	unsigned int n;
	int py, px; // pointer coordinates
	int ax, ay, aw, ah; // area position, width and height
	int center = 0, horizontal = 0, mirror = 0, fixed = 0; // layout configuration
	double fact;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	m = selmon;

	#if VANITYGAPS_PATCH
	int oh, ov, ih, iv;
	getgaps(m, &oh, &ov, &ih, &iv, &n);
	#else
	Client *c;
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	#endif // VANITYGAPS_PATCH

	ax = m->wx;
	ay = m->wy;
	ah = m->wh;
	aw = m->ww;

	if (!n)
		return;
	#if FLEXTILE_DELUXE_LAYOUT
	else if (m->lt[m->sellt]->arrange == &flextile) {
		int layout = m->ltaxis[LAYOUT];
		if (layout < 0) {
			mirror = 1;
			layout *= -1;
		}
		if (layout > FLOATING_MASTER) {
			layout -= FLOATING_MASTER;
			fixed = 1;
		}

		if (layout == SPLIT_HORIZONTAL || layout == SPLIT_HORIZONTAL_DUAL_STACK)
			horizontal = 1;
		else if (layout == SPLIT_CENTERED_VERTICAL && (fixed || n - m->nmaster > 1))
			center = 1;
		else if (layout == FLOATING_MASTER) {
			center = 1;
			if (aw < ah)
				horizontal = 1;
		}
		else if (layout == SPLIT_CENTERED_HORIZONTAL) {
			if (fixed || n - m->nmaster > 1)
				center = 1;
			horizontal = 1;
		}
	}
	#endif // FLEXTILE_DELUXE_LAYOUT
	#if CENTEREDMASTER_LAYOUT
	else if (m->lt[m->sellt]->arrange == &centeredmaster && (fixed || n - m->nmaster > 1))
		center = 1;
	#endif // CENTEREDMASTER_LAYOUT
	#if CENTEREDFLOATINGMASTER_LAYOUT
	else if (m->lt[m->sellt]->arrange == &centeredfloatingmaster)
		center = 1;
	#endif // CENTEREDFLOATINGMASTER_LAYOUT
	#if BSTACK_LAYOUT
	else if (m->lt[m->sellt]->arrange == &bstack)
		horizontal = 1;
	#endif // BSTACK_LAYOUT
	#if BSTACKHORIZ_LAYOUT
	else if (m->lt[m->sellt]->arrange == &bstackhoriz)
		horizontal = 1;
	#endif // BSTACKHORIZ_LAYOUT

	/* do not allow mfact to be modified under certain conditions */
	if (!m->lt[m->sellt]->arrange                            // floating layout
		|| (!fixed && m->nmaster && n <= m->nmaster)         // no master
		#if MONOCLE_LAYOUT
		|| m->lt[m->sellt]->arrange == &monocle
		#endif // MONOCLE_LAYOUT
		#if GRIDMODE_LAYOUT
		|| m->lt[m->sellt]->arrange == &grid
		#endif // GRIDMODE_LAYOUT
		#if HORIZGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &horizgrid
		#endif // HORIZGRID_LAYOUT
		#if GAPPLESSGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &gaplessgrid
		#endif // GAPPLESSGRID_LAYOUT
		#if NROWGRID_LAYOUT
		|| m->lt[m->sellt]->arrange == &nrowgrid
		#endif // NROWGRID_LAYOUT
		#if FLEXTILE_DELUXE_LAYOUT
		|| (m->lt[m->sellt]->arrange == &flextile && m->ltaxis[LAYOUT] == NO_SPLIT)
		#endif // FLEXTILE_DELUXE_LAYOUT
	)
		return;

	#if VANITYGAPS_PATCH
	ay += oh;
	ax += ov;
	aw -= 2*ov;
	ah -= 2*oh;
	#endif // VANITYGAPS_PATCH

	if (center) {
		if (horizontal) {
			px = ax + aw / 2;
			#if VANITYGAPS_PATCH
			py = ay + ah / 2 + (ah - 2*ih) * (m->mfact / 2.0) + ih / 2;
			#else
			py = ay + ah / 2 + ah * m->mfact / 2.0;
			#endif // VANITYGAPS_PATCH
		} else { // vertical split
			#if VANITYGAPS_PATCH
			px = ax + aw / 2 + (aw - 2*iv) * m->mfact / 2.0 + iv / 2;
			#else
			px = ax + aw / 2 + aw * m->mfact / 2.0;
			#endif // VANITYGAPS_PATCH
			py = ay + ah / 2;
		}
	} else if (horizontal) {
		px = ax + aw / 2;
		if (mirror)
			#if VANITYGAPS_PATCH
			py = ay + (ah - ih) * (1.0 - m->mfact) + ih / 2;
			#else
			py = ay + (ah * (1.0 - m->mfact));
			#endif // VANITYGAPS_PATCH
		else
			#if VANITYGAPS_PATCH
			py = ay + ((ah - ih) * m->mfact) + ih / 2;
			#else
			py = ay + (ah * m->mfact);
			#endif // VANITYGAPS_PATCH
	} else { // vertical split
		if (mirror)
			#if VANITYGAPS_PATCH
			px = ax + (aw - iv) * (1.0 - m->mfact) + iv / 2;
			#else
			px = ax + (aw * m->mfact);
			#endif // VANITYGAPS_PATCH
		else
			#if VANITYGAPS_PATCH
			px = ax + ((aw - iv) * m->mfact) + iv / 2;
			#else
			px = ax + (aw * m->mfact);
			#endif // VANITYGAPS_PATCH
		py = ay + ah / 2;
	}

	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[horizontal ? CurResizeVertArrow : CurResizeHorzArrow]->cursor, CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, root, 0, 0, 0, 0, px, py);

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 40))
				continue;
			if (lasttime != 0) {
				px = ev.xmotion.x;
				py = ev.xmotion.y;
			}
			lasttime = ev.xmotion.time;

			#if VANITYGAPS_PATCH
			if (center)
				if (horizontal)
					if (py - ay > ah / 2)
						fact = (double) 1.0 - (ay + ah - py - ih / 2) * 2 / (double) (ah - 2*ih);
					else
						fact = (double) 1.0 - (py - ay - ih / 2) * 2 / (double) (ah - 2*ih);
				else
					if (px - ax > aw / 2)
						fact = (double) 1.0 - (ax + aw - px - iv / 2) * 2 / (double) (aw - 2*iv);
					else
						fact = (double) 1.0 - (px - ax - iv / 2) * 2 / (double) (aw - 2*iv);
			else
				if (horizontal)
					fact = (double) (py - ay - ih / 2) / (double) (ah - ih);
				else
					fact = (double) (px - ax - iv / 2) / (double) (aw - iv);
			#else
			if (center)
				if (horizontal)
					if (py - ay > ah / 2)
						fact = (double) 1.0 - (ay + ah - py) * 2 / (double) ah;
					else
						fact = (double) 1.0 - (py - ay) * 2 / (double) ah;
				else
					if (px - ax > aw / 2)
						fact = (double) 1.0 - (ax + aw - px) * 2 / (double) aw;
					else
						fact = (double) 1.0 - (px - ax) * 2 / (double) aw;
			else
				if (horizontal)
					fact = (double) (py - ay) / (double) ah;
				else
					fact = (double) (px - ax) / (double) aw;
			#endif // VANITYGAPS_PATCH

			if (!center && mirror)
				fact = 1.0 - fact;

			setmfact(&((Arg) { .f = 1.0 + fact }));
			px = ev.xmotion.x;
			py = ev.xmotion.y;
			break;
		}
	} while (ev.type != ButtonRelease);

	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
drawbar(Monitor *m)
{
	int indn;
	int x, w, tw = 0, stw = 0, etw = 0, n = 0, scm;
	int boxs = drw->fonts->h / 9;
	int boxw = drw->fonts->h / 6 + 2;
	unsigned int i, occ = 0, urg = 0;
	Client *c;
	char tagdisp[64];
	char *masterclientontag[LENGTH(tags)];
	char alttagdisp[64];
	char *altmasterclientontag[LENGTH(tags)];
	Fnt *cur;

	if(showsystray && m == systraytomon(m) && !systrayonleft)
		stw = getsystraywidth();

	/* draw status first so it can be overdrawn by tags later */
	if (m == selmon) { /* status is only drawn on selected monitor */
		cur = drw->fonts; // remember which was the first font
		drw->fonts = drw->fonts->next; // skip to the second font, add more of these to get to third, fourth etc.
		tw = statusw = m->ww - drawstatusbar(m, bh, stext, 0);
		drw->fonts = cur; // set the normal font back to the first font
	}

	resizebarwin(m);
	for (i = 0; i < LENGTH(tags); i++) {
		masterclientontag[i] = NULL;
		altmasterclientontag[i] = NULL;
	}

	for (c = m->clients; c; c = c->next) {
		if (ISVISIBLE(c))
			n++;
		occ |= c->tags == 255 ? 0 : c->tags;
		if (c->isurgent)
			urg |= c->tags;
		if (!selmon->alttag) {
			for (i = 0; i < LENGTH(tags); i++)
				if (!masterclientontag[i] && c->tags & (1<<i)) {
					XClassHint ch = { NULL, NULL };
					XGetClassHint(dpy, c->win, &ch);
					masterclientontag[i] = ch.res_class;
					if (lcaselbl)
					masterclientontag[i][0] = tolower(masterclientontag[i][0]);
				}
		} else {
			for (i = 0; i < LENGTH(tags); i++)
				if (!altmasterclientontag[i] && c->tags & (1<<i)) {
					XClassHint ch = { NULL, NULL };
					XGetClassHint(dpy, c->win, &ch);
					altmasterclientontag[i] = ch.res_class;
					if (altlcaselbl)
					altmasterclientontag[i][0] = tolower(altmasterclientontag[i][0]);
				}
		}
	}
	x = 0;
	for (i = 0; i < LENGTH(tags); i++) {
		indn = 0;
		/* do not draw vacant tags */
		if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
		continue;

		if (!selmon->alttag) {
			if (masterclientontag[i])
				snprintf(tagdisp, 64, ptagf, (occ & 1 << i ? occtags[i] : tags[i]), masterclientontag[i]);
			else
				snprintf(tagdisp, 64, etagf, (occ & 1 << i ? occtags[i] : tags[i]));
			masterclientontag[i] = tagdisp;
			tagw[i] = w = TEXTW(masterclientontag[i]);
		} else {
			if (altmasterclientontag[i])
				snprintf(alttagdisp, 64, altptagf, (occ & 1 << i ? occtagsalt[i] : tagsalt[i]), altmasterclientontag[i]);
			else
				snprintf(alttagdisp, 64, altetagf, (occ & 1 << i ? occtagsalt[i] : tagsalt[i]));
			altmasterclientontag[i] = alttagdisp;
			alttagw[i] = w = TEXTW(altmasterclientontag[i]);
		}
		drw_setscheme(drw, (m->tagset[m->seltags] & 1 << i ? tagscheme[i] : scheme[SchemeNorm]));
		drw_text(drw, x, 0, w, bh, lrpad / 2, (selmon->alttag ? altmasterclientontag[i] : masterclientontag[i]), urg & 1 << i);
		if (ulineall || m->tagset[m->seltags] & 1 << i) /* if there are conflicts, just move these lines directly underneath both 'drw_setscheme' and 'drw_text' :) */
			drw_rect(drw, x + ulinepad, bh - ulinestroke - ulinevoffset, w - (ulinepad * 2), ulinestroke, 1, 0);
		for (c = m->clients; c; c = c->next) {
			if (c->tags & (1 << i)) {
				drw_rect(drw, x, 1 + (indn * 2), selmon->sel == c ? 6 : 1, 1, 1, urg & 1 << i);
				indn++;
			}
		}
		x += w;
	}
	w = blw = TEXTW(m->ltsymbol);
	drw_setscheme(drw, scheme[SchemeNorm]);
	x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
	
	char attachmodestr[64];
	switch(attachmode % 4){
		case 0: strcpy(attachmodestr, "master");
			break;
		case 1: strcpy(attachmodestr, "above");
			break;
		case 2: strcpy(attachmodestr, "aside");
			break;
		case 3: strcpy(attachmodestr, "below");
			break;
		case 4: strcpy(attachmodestr, "bottom");
			break;
	}
	if (showattachmodestr) {
		drw_setscheme(drw, scheme[SchemeAttach]);
		x = drw_text(drw, x, 0, TEXTW(attachmodestr), bh, lrpad / 2, attachmodestr, 0);
	}

	/* Draw swalsymbol next to ltsymbol. */
	if (m->sel && m->sel->swallowedby) {
		w = TEXTW(swalsymbol);
		x = drw_text(drw, x, 0, w, bh, lrpad / 2, swalsymbol, 0);
	}

	unsigned int columnstmp = (LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0)) * bh / tagrows;
	int xx = m->ww - tw - columnstmp + 1;
	if (drawtagmask & DRAWTAGGRID) {
		drawtaggrid(m,&xx,occ);
	}

	if ((w = m->ww - tw - x) > bh) {
		if (m->sel) {
            /* fix overflow when window name is bigger than window width */
			int mid = (w - (int)TEXTW(m->sel->name) - (m->sel->icon ? ICONSPACING + m->sel->icw : 0) + lrpad - columnstmp + 1) / 2;
			/* make sure name will not overlap on tags even when it is very long */
			mid = ((m->sel->icon ? ICONSPACING + m->sel->icw : 0) + (int)TEXTW(m->sel->name)) > (m->ww - stw - 2 * sp - tw - x) ? lrpad / 2 : mid;
			drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
			drw_text(drw, x, 0, w - columnstmp + 1, bh, mid + (m->sel->icon ? m->sel->icw + ICONSPACING : 0), m->sel->name, 0);
			if (m->sel->icon) drw_pic(drw, x + mid, (bh - m->sel->ich) / 2, m->sel->icw, m->sel->ich, m->sel->icon);
			if (m->sel->isfloating) {
				drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
				if (m->sel->isalwaysontop)
					drw_rect(drw, x + boxs, bh - boxw, boxw, boxw, 0, 0);
			}
			if (m->sel->issticky)
				drw_polygon(drw, x + boxs, m->sel->isfloating ? boxs * 2 + boxw : boxs, stickyiconbb.x, stickyiconbb.y, boxw, boxw * stickyiconbb.y / stickyiconbb.x, stickyicon, LENGTH(stickyicon), Nonconvex, m->sel->tags & m->tagset[m->seltags]);
		} else {
			drw_setscheme(drw, scheme[SchemeNorm]);
			drw_rect(drw, x, 0, w - columnstmp + 1, bh, 1, 1);
		}
	}
	drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);

	if (m == selmon) { /* extra status is only drawn on selected monitor */
		drw_setscheme(drw, scheme[SchemeNorm]);
		/* clear default bar draw buffer by drawing a blank rectangle */
		drw_rect(drw, 0, 0, m->ww, bh, 1, 1);
		cur = drw->fonts; // remember which was the first font
		drw->fonts = drw->fonts->next; // skip to the second font, add more of these to get to third, fourth etc.
		etw = statusew = extrabarright ? m->ww - drawstatusbar(m, bh, estext, 1) : drawstatusbar(m, bh, estext, 1);
		drw->fonts = cur; // set the normal font back to the first font
		if (n > 0) {
			int remainder = w % n;
			int exw = extrabarright ? 0 : etw;
			int tabw = (1.0 / (double)n) * (m->ww - etw - 2 * sp);
			for (c = m->clients; c; c = c->next) {
				if (!ISVISIBLE(c))
					continue;
				if (m->sel == c)
					scm = SchemeSel;
				else if (HIDDEN(c))
					scm = SchemeHid;
				else
					scm = SchemeNorm;
				drw_setscheme(drw, scheme[scm]);

				if (remainder >= 0) {
					if (remainder == 0) {
						tabw--;
					}
					remainder--;
				}
				drw_text(drw, exw, 0, tabw, bh, lrpad / 2, c->name, 0);
				exw += tabw;
			}
		}
		m->bt = n;
		m->btw = w;
		drw_map(drw, m->extrabarwin, 0, 0, m->ww, bh);
	}
}

void
drawbars(void)
{
	Monitor *m;

	for (m = mons; m; m = m->next)
		drawbar(m);
}
void drawtaggrid(Monitor *m, int *x_pos, unsigned int occ)
{
    unsigned int x, y, h, max_x, columns;
    int invert, i,j, k;

    h = bh / tagrows;
    x = max_x = *x_pos;
    y = 0;
    columns = LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);

    /* Firstly we will fill the borders of squares */

    XSetForeground(drw->dpy, drw->gc, scheme[SchemeNorm][ColBorder].pixel);
    XFillRectangle(dpy, drw->drawable, drw->gc, x, y, h*columns + 1, bh);

    /* We will draw LENGTH(tags) squares in tagraws raws. */
	for(j = 0,  i= 0; j < tagrows; j++) {
        x = *x_pos;
        for (k = 0; k < columns && i < LENGTH(tags); k++, i++) {
		    invert = m->tagset[m->seltags] & 1 << i ? 0 : 1;

            /* Select active color for current square */
            XSetForeground(drw->dpy, drw->gc, !invert ? scheme[SchemeSel][ColBg].pixel :
                                scheme[SchemeNorm][ColFg].pixel);
            XFillRectangle(dpy, drw->drawable, drw->gc, x+1, y+1, h-1, h-1);

            /* Mark square if tag has client */
            if (occ & 1 << i) {
                XSetForeground(drw->dpy, drw->gc, !invert ? scheme[SchemeSel][ColFg].pixel :
                                scheme[SchemeNorm][ColBg].pixel);
                XFillRectangle(dpy, drw->drawable, drw->gc, x + 1, y + 1,
                               h / 2, h / 2);
            }
		    x += h;
            if (x > max_x) {
                max_x = x;
            }
        }
        y += h;
	}
    *x_pos = max_x + 1;
}

void
distributetags(const Arg *arg)
{
        unsigned int ui = 1;
        int i = 0;
        for (Client *c = selmon->clients; c; c = c->next) {
                c->tags = (ui << i) & TAGMASK;
                i = (i + 1) % LENGTH(tags);
        }
        focus(NULL);
        arrange(selmon);
}
void
drawtabs(void) {
	Monitor *m;

	for(m = mons; m; m = m->next)
		drawtab(m);
}

static int
cmpint(const void *p1, const void *p2) {
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */
  return *((int*) p1) > * (int*) p2;
}

void
drawtab(Monitor *m) {
	Client *c;
	int i;
	int itag = -1;
	char view_info[50];
	int view_info_w = 0;
	int sorted_label_widths[MAXTABS];
	int tot_width;
	int maxsize = bh;
	int x = 0;
	int w = 0;

	//view_info: indicate the tag which is displayed in the view
	for(i = 0; i < LENGTH(tags); ++i){
	  if((selmon->tagset[selmon->seltags] >> i) & 1) {
	    if(itag >=0){ //more than one tag selected
	      itag = -1;
	      break;
	    }
	    itag = i;
	  }
	}

	if(0 <= itag  && itag < LENGTH(tags)){
	  snprintf(view_info, sizeof view_info, "[%s]", tags[itag]);
	} else {
	  strncpy(view_info, "[...]", sizeof view_info);
	}
	view_info[sizeof(view_info) - 1 ] = 0;
	view_info_w = TEXTW(view_info);
	tot_width = view_info_w;

	/* Calculates number of labels and their width */
	m->ntabs = 0;
	for(c = m->clients; c; c = c->next){
	  if(!ISVISIBLE(c)) continue;
	  m->tab_widths[m->ntabs] = TEXTW(c->name);
	  tot_width += m->tab_widths[m->ntabs];
	  ++m->ntabs;
	  if(m->ntabs >= MAXTABS) break;
	}

	if(tot_width > m->ww){ //not enough space to display the labels, they need to be truncated
	  memcpy(sorted_label_widths, m->tab_widths, sizeof(int) * m->ntabs);
	  qsort(sorted_label_widths, m->ntabs, sizeof(int), cmpint);
	  tot_width = view_info_w;
	  for(i = 0; i < m->ntabs; ++i){
	    if(tot_width + (m->ntabs - i) * sorted_label_widths[i] > m->ww)
	      break;
	    tot_width += sorted_label_widths[i];
	  }
	  maxsize = (m->ww - tot_width) / (m->ntabs - i);
	} else{
	  maxsize = m->ww;
	}
	i = 0;
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, m->ww, th, 1, 1);
	for(c = m->clients; c; c = c->next){
	  if(!ISVISIBLE(c)) continue;
	  if(i >= m->ntabs) break;
	  if(m->tab_widths[i] >  maxsize) m->tab_widths[i] = maxsize;
	  w = m->tab_widths[i];
	  drw_setscheme(drw, scheme[(c == m->sel) ? SchemeSel : SchemeNorm]);
	  drw_text(drw, x + horizpadbar / 2, vertpadbar / 2, w, th - vertpadbar, lrpad / 2, c->name, 0);
	  x += w;
	  ++i;
	}
	x += horizpadbar / 2;

	int buttons_w = 0;
	buttons_w += TEXTW2(btn_prev) + horizpadbar;
	buttons_w += TEXTW2(btn_next) + horizpadbar;
	buttons_w += TEXTW2(btn_close) + horizpadbar;
	/* cleans interspace between window names and current viewed tag label */
	w = m->ww - view_info_w - x - buttons_w;
	drw_setscheme(drw, scheme[SchemeNorm]);
	if(TEXTW2(tabstatus) <= (m->ww - x - buttons_w - view_info_w - horizpadbar / 2)) {
		if(!tabstatuscenter)
			drw_text(drw, x, 0, w, th, 0, tabstatus, 0);
		else 
			drw_text(drw, x + (m->ww - x - buttons_w - view_info_w - horizpadbar / 2 - TEXTW2(tabstatus)) / 2, 0, w, th, 0, tabstatus, 0);
	} else {
			drw_text(drw, x, 0, w, th, 0, "", 0);
	}
	x += w;
	
	w = m->ww - x - buttons_w - view_info_w;
	x += w;

	aftertabx = x;
	w = viewwidth =  view_info_w - lrpad;
	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_text(drw, x, 0, w, th, 0, view_info, 0);
	x += w;

	drw_setscheme(drw, scheme[SchemePrev]);
	w = TEXTW2(btn_prev) + horizpadbar;
	drw_text(drw, x + horizpadbar / 2, vertpadbar / 2, w, th - vertpadbar, horizpadbar / 2, btn_prev, 0);
	x += w;

	drw_setscheme(drw, scheme[SchemeNext]);
	w = TEXTW2(btn_next) + horizpadbar;
	drw_text(drw, x + horizpadbar / 2, vertpadbar / 2, w, th - vertpadbar, horizpadbar / 2, btn_next, 0);
	x += w;

	drw_setscheme(drw, scheme[SchemeClose]);
	w = TEXTW2(btn_close) + horizpadbar;
	drw_text(drw, x + horizpadbar / 2, vertpadbar / 2, w, th - vertpadbar, horizpadbar / 2, btn_close, 0);
	x += w;

	drw_map(drw, m->tabwin, 0, 0, m->ww, th);
}


void
enternotify(XEvent *e)
{
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	c = wintoclient(ev->window);
	m = c ? c->mon : wintomon(ev->window);
	if (m != selmon) {
		unfocus(selmon->sel, 1);
		prevmon = selmon;
		selmon = m;
	} else if (!c || c == selmon->sel)
		return;
	focus(c);
}

void
expose(XEvent *e)
{
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (m = wintomon(ev->window))) {
		drawbar(m);
		drawtab(m);
		if (m == selmon)
			updatesystray();
	}
}

int
fakesignal(void)
{
	/* Command syntax: <PREFIX><COMMAND>[<SEP><ARG>]... */
	static const char sep[] = "###";
	static const char prefix[] = "#!";

	size_t numsegments, numargs;
	char rootname[256];
	char *segments[16] = {0};

	/* Get root name, split by separator and find the prefix */
	if (!gettextprop(root, XA_WM_NAME, rootname, sizeof(rootname))
		|| strncmp(rootname, prefix, sizeof(prefix) - 1)) {
		return 0;
	}
	numsegments = split(rootname + sizeof(prefix) - 1, sep, segments, sizeof(segments));
	numargs = numsegments - 1; /* number of arguments to COMMAND */

	if (!strcmp(segments[0], "swalreg")) {
		/* Params: windowid, [class], [instance], [title] */
		Window w;
		Client *c;

		if (numargs >= 1) {
			w = strtoul(segments[1], NULL, 0);
			switch (wintoclient2(w, &c, NULL)) {
			case ClientRegular: /* fallthrough */
			case ClientSwallowee:
				swalreg(c, segments[2], segments[3], segments[4]);
				break;
			}
		}
	}
	else if (!strcmp(segments[0], "swal")) {
		/* Params: swallower's windowid, swallowee's window-id */
		Client *swer, *swee;
		Window winswer, winswee;
		int typeswer, typeswee;

		if (numargs >= 2) {
			winswer = strtoul(segments[1], NULL, 0);
			typeswer = wintoclient2(winswer, &swer, NULL);
			winswee = strtoul(segments[2], NULL, 0);
			typeswee = wintoclient2(winswee, &swee, NULL);
			if ((typeswer == ClientRegular || typeswer == ClientSwallowee)
				&& (typeswee == ClientRegular || typeswee == ClientSwallowee))
				swal(swer, swee, 0);
		}
	}
	else if (!strcmp(segments[0], "swalunreg")) {
		/* Params: swallower's windowid */
		Client *swer;
		Window winswer;

		if (numargs == 1) {
			winswer = strtoul(segments[1], NULL, 0);
			if ((swer = wintoclient(winswer)))
				swalunreg(swer);
		}
	}
	else if (!strcmp(segments[0], "swalstop")) {
		/* Params: swallowee's windowid */
		Client *swee;
		Window winswee;

		if (numargs == 1) {
			winswee = strtoul(segments[1], NULL, 0);
			if ((swee = wintoclient(winswee)))
				swalstop(swee, NULL);
		}
	}
	return 1;
}

Client *
findbefore(Client *c)
{
	Client *tmp;
	if (c == selmon->clients)
		return NULL;
	for (tmp = selmon->clients; tmp && tmp->next != c; tmp = tmp->next);
	return tmp;
}

void
focus(Client *c)
{
	if (!c || !ISVISIBLE(c))
		for (c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext);
	if (selmon->sel && selmon->sel != c) {
		unfocus(selmon->sel, 0);

		if (selmon->hidsel) {
			hidewin(selmon->sel);
			if (c)
				arrange(c->mon);
			selmon->hidsel = 0;
		}
	}
	if (c) {
		if (c->cantfocus)
			return;
		if (!c->canfocus)
		    return;
		if (c->mon != selmon) {
			prevmon = selmon;
			selmon = c->mon;
		}
		if (c->isurgent)
			seturgent(c, 0);
		detachstack(c);
		attachstack(c);
		grabbuttons(c, 1);
		if (c == mark)
			XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColMark].pixel);
		else
			XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
		setfocus(c);
		opacity(c, activeopacity);
	} else {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
	selmon->sel = c;
	drawbars();
	drawtabs();
}

/* there are some broken focus acquiring clients needing extra handling */
void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (selmon->sel && ev->window != selmon->sel->win)
		setfocus(selmon->sel);
}

void
focusmaster(const Arg *arg)
{
	Client *master;

	if (selmon->nmaster > 1)
		return;
	if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
		return;

	master = nexttiled(selmon->clients);

	if (!master)
		return;

	int i;
	for (i = 0; !(selmon->tagset[selmon->seltags] & 1 << i); i++);
	i++;

	if (selmon->sel == master) {
		if (selmon->tagmarked[i] && ISVISIBLE(selmon->tagmarked[i]))
			focus(selmon->tagmarked[i]);
	} else {
		selmon->tagmarked[i] = selmon->sel;
		focus(master);
	}
}

void
focusmon(const Arg *arg)
{
	Monitor *m;

	if (!mons->next)
		return;
	if ((m = dirtomon(arg->i)) == selmon)
		return;
	unfocus(selmon->sel, 0);
	prevmon = selmon;
	selmon = m;
	focus(NULL);
}

void
focusstackvis(const Arg *arg)
{
	focusstack(arg->i, 0);
}

void
focusstackhid(const Arg *arg)
{
	focusstack(arg->i, 1);
}

void
focusstack(int inc, int hid)
{
	Client *c = NULL, *i;

	if ((!selmon->sel && !hid) || (selmon->sel->isfullscreen && lockfullscreen && !hid))
		return;
	if (!selmon->clients)
		return;

	if (inc > 0) {
		if (selmon->sel)
			for (c = selmon->sel->next;
					 c && (!ISVISIBLE(c) || (!hid && HIDDEN(c)) || !c->canfocus || c->cantfocus);
					 c = c->next);
		if (!c)
			for (c = selmon->clients;
					 c && (!ISVISIBLE(c) || (!hid && HIDDEN(c)) || !c->canfocus || c->cantfocus);
					 c = c->next);
	} else {
		if (selmon->sel) {
			for (i = selmon->clients; i != selmon->sel; i = i->next)
				if (ISVISIBLE(i) && !(!hid && HIDDEN(i)) && i->canfocus && !i->cantfocus)
					c = i;
		} else
			c = selmon->clients;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i) && !(!hid && HIDDEN(i)) && i->canfocus && !i->cantfocus)
					c = i;
	}
	if (c) {
		focus(c);
		restack(selmon);

		if (HIDDEN(c)) {
			showwin(c);
			c->mon->hidsel = 1;
		}
	}
}

void
focuswin(const Arg* arg){
  int iwin = arg->i;
  Client* c = NULL;
  for(c = selmon->clients; c && (iwin || !ISVISIBLE(c)) ; c = c->next){
    if(ISVISIBLE(c)) --iwin;
  };
  if(c) {
    focus(c);
    restack(selmon);
  }
}

Atom
getatomprop(Client *c, Atom prop)
{
	int di;
	unsigned long dl;
	unsigned char *p = NULL;
	Atom da, atom = None;
	/* FIXME getatomprop should return the number of items and a pointer to
	 * the stored data instead of this workaround */
	Atom req = XA_ATOM;
	if (prop == xatom[XembedInfo])
		req = xatom[XembedInfo];

	if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
		&da, &di, &dl, &dl, &p) == Success && p) {
		atom = *(Atom *)p;
		if (da == xatom[XembedInfo] && dl == 2)
			atom = ((Atom *)p)[1];
		XFree(p);
	}
	return atom;
}

pid_t
getstatusbarpid()
{
	char buf[32], *str = buf, *c;
	FILE *fp;

	if (statuspid > 0) {
		snprintf(buf, sizeof(buf), "/proc/%u/cmdline", statuspid);
		if ((fp = fopen(buf, "r"))) {
			fgets(buf, sizeof(buf), fp);
			while ((c = strchr(str, '/')))
				str = c + 1;
			fclose(fp);
			if (!strcmp(str, STATUSBAR))
				return statuspid;
		}
	}
	if (!(fp = popen("pidof -s "STATUSBAR, "r")))
		return -1;
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
	return strtoul(buf, NULL, 10);
}

static uint32_t prealpha(uint32_t p) {
	uint8_t a = p >> 24u;
	uint32_t rb = (a * (p & 0xFF00FFu)) >> 8u;
	uint32_t g = (a * (p & 0x00FF00u)) >> 8u;
	return (rb & 0xFF00FFu) | (g & 0x00FF00u) | (a << 24u);
}

Picture
geticonprop(Window win, unsigned int *picw, unsigned int *pich)
{
	int format;
	unsigned long n, extra, *p = NULL;
	Atom real;

	if (XGetWindowProperty(dpy, win, netatom[NetWMIcon], 0L, LONG_MAX, False, AnyPropertyType, 
						   &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return None; 
	if (n == 0 || format != 32) { XFree(p); return None; }

	unsigned long *bstp = NULL;
	uint32_t w, h, sz;
	{
		unsigned long *i; const unsigned long *end = p + n;
		uint32_t bstd = UINT32_MAX, d, m;
		for (i = p; i < end - 1; i += sz) {
			if ((w = *i++) > UINT16_MAX || (h = *i++) > UINT16_MAX) { XFree(p); return None; }
			if ((sz = w * h) > end - i) break;
			if ((m = w > h ? w : h) >= ICONSIZE && (d = m - ICONSIZE) < bstd) { bstd = d; bstp = i; }
		}
		if (!bstp) {
			for (i = p; i < end - 1; i += sz) {
				if ((w = *i++) > UINT16_MAX || (h = *i++) > UINT16_MAX) { XFree(p); return None; }
				if ((sz = w * h) > end - i) break;
				if ((d = ICONSIZE - (w > h ? w : h)) < bstd) { bstd = d; bstp = i; }
			}
		}
		if (!bstp) { XFree(p); return None; }
	}

	if ((w = *(bstp - 2)) == 0 || (h = *(bstp - 1)) == 0) { XFree(p); return None; }

	uint32_t icw, ich;
	if (w <= h) {
		ich = ICONSIZE; icw = w * ICONSIZE / h;
		if (icw == 0) icw = 1;
	}
	else {
		icw = ICONSIZE; ich = h * ICONSIZE / w;
		if (ich == 0) ich = 1;
	}
	*picw = icw; *pich = ich;

	uint32_t i, *bstp32 = (uint32_t *)bstp;
	for (sz = w * h, i = 0; i < sz; ++i) bstp32[i] = prealpha(bstp[i]);

	Picture ret = drw_picture_create_resized(drw, (char *)bstp, w, h, icw, ich);
	XFree(p);

	return ret;
}

pid_t
getparentprocess(pid_t p)
{
	unsigned int v = 0;

#ifdef __linux__
	FILE *f;
	char buf[256];
	snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

	if (!(f = fopen(buf, "r")))
		return 0;

	fscanf(f, "%*u %*s %*c %u", &v);
	fclose(f);
#endif /* __linux__*/

#ifdef __OpenBSD__
	int n;
	kvm_t *kd;
	struct kinfo_proc *kp;

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, NULL);
	if (!kd)
		return 0;

	kp = kvm_getprocs(kd, KERN_PROC_PID, p, sizeof(*kp), &n);
	v = kp->p_ppid;
#endif /* __OpenBSD__ */

	return (pid_t)v;
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w)
{
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
		&real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

unsigned int
getsystraywidth()
{
	unsigned int w = 0;
	Client *i;
	if(showsystray)
		for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
	return w ? w + systrayspacing : 1;
}

int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return 0;
	text[0] = '\0';
	if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
		return 0;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return 1;
}

void
goback(const Arg *arg)
{
	if (prevmon == NULL) {
		Arg a = {0};
		view(&a);
	} else if (prevmon != selmon) {
		unfocus(selmon->sel, 0);
		Monitor *p = selmon;
		selmon = prevmon;
		focus(NULL);
		prevmon = p;
	}
}

void
grabbuttons(Client *c, int focused)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if (!focused)
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
				BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == ClkClientWin)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button,
						buttons[i].mask | modifiers[j],
						c->win, False, BUTTONMASK,
						GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void)
{
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;
		KeyCode chain;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for (i = 0; i < LENGTH(keys); i++)
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym))) {
				if (keys[i].chain != -1 &&
					((chain = XKeysymToKeycode(dpy, keys[i].chain))))
						code = chain;
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
			}
	}
}

void
hide(const Arg *arg)
{
	hidewin(selmon->sel);
	focus(NULL);
	arrange(selmon);
}

void
hidewin(Client *c) {
	if (!c || HIDDEN(c))
		return;

	Window w = c->win;
	static XWindowAttributes ra, ca;

	// more or less taken directly from blackbox's hide() function
	XGrabServer(dpy);
	XGetWindowAttributes(dpy, root, &ra);
	XGetWindowAttributes(dpy, w, &ca);
	// prevent UnmapNotify events
	XSelectInput(dpy, root, ra.your_event_mask & ~SubstructureNotifyMask);
	XSelectInput(dpy, w, ca.your_event_mask & ~StructureNotifyMask);
	XUnmapWindow(dpy, w);
	setclientstate(c, IconicState);
	XSelectInput(dpy, root, ra.your_event_mask);
	XSelectInput(dpy, w, ca.your_event_mask);
	XUngrabServer(dpy);

	focus(c->snext);
	arrange(c->mon);
}

int
handlexevent(struct epoll_event *ev)
{
	if (ev->events & EPOLLIN) {
		XEvent ev;
		while (running && XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (handler[ev.type]) {
				handler[ev.type](&ev); /* call handler */
				ipc_send_events(mons, &lastselmon, selmon);
			}
		}
	} else if (ev-> events & EPOLLHUP) {
		return -1;
	}

	return 0;
}

void
incnmaster(const Arg *arg)
{
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

int
isdescprocess(pid_t p, pid_t c)
{
	while (p != c && c != 0)
		c = getparentprocess(c);

	return (int)c;
}

#ifdef XINERAMA
static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
	while (n--)
		if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
		&& unique[n].width == info->width && unique[n].height == info->height)
			return 0;
	return 1;
}
#endif /* XINERAMA */

void
keypress(XEvent *e)
{
	unsigned int i, j;
	KeySym keysym;
	XKeyEvent *ev;
	int current = 0;
	unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++) {
		if (keysym == keys[i].keysym && keys[i].chain == -1
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
				&& keys[i].func)
			keys[i].func(&(keys[i].arg));
		else if (keysym == keys[i].chain && keychain == -1
				&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
				&& keys[i].func) {
			current = 1;
			keychain = keysym;
			for (j = 0; j < LENGTH(modifiers); j++)
				XGrabKey(dpy, AnyKey, 0 | modifiers[j], root,
						True, GrabModeAsync, GrabModeAsync);
		} else if (!current && keysym == keys[i].keysym
				&& keychain != -1
				&& keys[i].chain == keychain
				&& keys[i].func)
			keys[i].func(&(keys[i].arg));
	}
	if (!current) {
		keychain = -1;
		grabkeys();
	}
}

void
keyrelease(XEvent *e)
{
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);

    for (i = 0; i < LENGTH(keys); i++)
        if (momentaryalttags
        && keys[i].func && keys[i].func == togglealttag
        && selmon->alttag
        && (keysym == keys[i].keysym
        || CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)))
            keys[i].func(&(keys[i].arg));
}

int
fake_signal(void)
{
	char fsignal[256];
	char indicator[9] = "fsignal:";
	char str_signum[16];
	int i, v, signum;
	size_t len_fsignal, len_indicator = strlen(indicator);

	// Get root name property
	if (gettextprop(root, XA_WM_NAME, fsignal, sizeof(fsignal))) {
		len_fsignal = strlen(fsignal);

		// Check if this is indeed a fake signal
		if (len_indicator > len_fsignal ? 0 : strncmp(indicator, fsignal, len_indicator) == 0) {
			memcpy(str_signum, &fsignal[len_indicator], len_fsignal - len_indicator);
			str_signum[len_fsignal - len_indicator] = '\0';

			// Convert string value into managable integer
			for (i = signum = 0; i < strlen(str_signum); i++) {
				v = str_signum[i] - '0';
				if (v >= 0 && v <= 9) {
					signum = signum * 10 + v;
				}
			}

			// Check if a signal was found, and if so handle it
			if (signum)
				for (i = 0; i < LENGTH(signals); i++)
					if (signum == signals[i].signum && signals[i].func)
						signals[i].func(&(signals[i].arg));

			// A fake signal was sent
			return 1;
		}
	}

	// No fake signal was sent, so proceed with update
	return 0;
}

void
killclient(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
		XGrabServer(dpy);
		XSetErrorHandler(xerrordummy);
		XSetCloseDownMode(dpy, DestroyAll);
		XKillClient(dpy, selmon->sel->win);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
}

void
killunsel(const Arg *arg)
{
	Client *i = NULL;

	if (!selmon->sel)
		return;

	for (i = selmon->clients; i; i = i->next) {
		if (ISVISIBLE(i) && i != selmon->sel) {
			if (!sendevent(i->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
				XGrabServer(dpy);
				XSetErrorHandler(xerrordummy);
				XSetCloseDownMode(dpy, DestroyAll);
				XKillClient(dpy, i->win);
				XSync(dpy, False);
				XSetErrorHandler(xerror);
				XUngrabServer(dpy);
			}
		}
	}
}

void
layoutmenu(const Arg *arg) {
	FILE *p;
	char c[3], *s;
	int i;

	if (!(p = popen(layoutmenu_cmd, "r")))
		 return;
	s = fgets(c, sizeof(c), p);
	pclose(p);

	if (!s || *s == '\0' || c == '\0')
		 return;

	i = atoi(c);
	setlayout(&((Arg) { .v = &layouts[i] }));
}

void
manage(Window w, XWindowAttributes *wa)
{
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	c = ecalloc(1, sizeof(Client));
	c->win = w;
	c->pid = winpid(w);
	/* geometry */
	c->x = c->oldx = wa->x;
	c->y = c->oldy = wa->y;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	c->cfact = 1.0;

	updateicon(c);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
		c->mon = t->mon;
		c->tags = t->tags;
	} else {
		c->mon = selmon;
		applyrules(c);
	}

	if (unmanaged) {
		XMapWindow(dpy, c->win);
		if (unmanaged == 1)
			XRaiseWindow(dpy, c->win);
		else if (unmanaged == 2)
			XLowerWindow(dpy, c->win);
		free(c);
		unmanaged = 0;
		return;
	}

	if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
		c->x = c->mon->mx + c->mon->mw - WIDTH(c);
	if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
		c->y = c->mon->my + c->mon->mh - HEIGHT(c);
	c->x = MAX(c->x, c->mon->mx);
	/* only fix client y-offset, if the client center might cover the bar */
	c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
		&& (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
	c->bw = c->mon->borderpx;

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	if (c == mark)
		XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColMark].pixel);
	else
		XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
	configure(c); /* propagates border_width, if size doesn't change */
	updatewindowtype(c);
	updatesizehints(c);
	updatewmhints(c);
	updatemotifhints(c);
	if (c->iscentered) {
		c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
		c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
	}
	c->sfx = c->x;
	c->sfy = c->y;
	c->sfw = c->w;
	c->sfh = c->h;
	XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	grabbuttons(c, 0);
	if (!c->isfloating)
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if (c->isfloating)
		XRaiseWindow(dpy, c->win);
	attachx(c);
	attachstack(c);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(c->win), 1);
	XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
	if (!HIDDEN(c))
		setclientstate(c, NormalState);
	if (c->mon == selmon) {
		unfocus(selmon->sel, 0);
	}
	c->mon->sel = c;

	if (riopid && (!riodraw_matchpid || isdescprocess(riopid, c->pid))) {
		if (riodimensions[3] != -1)
			rioposition(c, riodimensions[0], riodimensions[1], riodimensions[2], riodimensions[3]);
		else {
			killclient(&((Arg) { .v = c }));
			return;
		}
	}
	arrange(c->mon);
	if (!HIDDEN(c))
		XMapWindow(dpy, c->win);
	focus(NULL);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	Client *c, *swee, *root;
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;
	Client *i;
	Swallow *s;
	if ((i = wintosystrayicon(ev->window))) {
		sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
		resizebarwin(selmon);
		updatesystray();
	}

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	switch (wintoclient2(ev->window, &c, &root)) {
	case ClientRegular: /* fallthrough */
	case ClientSwallowee:
		/* Regulars and swallowees are always mapped. Nothing to do. */
		break;
	case ClientSwallower:
		/* Remapping a swallower will simply stop the swallow. */
		for (swee = root; swee->swallowedby != c; swee = swee->swallowedby);
		swalstop(swee, root);
		break;
	default:
		/* No client is managing the window. See if any swallows match. */
		if ((s = swalmatch(ev->window)))
			swalmanage(s, ev->window, &wa);
		else
			manage(ev->window, &wa);
		break;
	}

	/* Reduce decay counter of all swallow instances. */
	if (swaldecay)
		swaldecayby(1);
}

void
monocle(Monitor *m)
{
	unsigned int n = 0;
	Client *c;

	if (!topbar) {
		vp = -vp;
	}
	for (c = m->clients; c; c = c->next)
		if (ISVISIBLE(c))
			n++;
	if (n > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
	for (c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
		if (n == 1) {
			resize(c, m->wx + sp, m->wy + vp + gappoh, m->ww - 2 * c->bw - 2 * sp, m->wh - 2 * c->bw - 2 * vp - 2 * gappoh, 0);
		} else {
			if(m->showtab == showtab_always
			   || (m->showtab == showtab_auto)) {
				if (!toptab)
					resize(c, m->wx + sp, m->wy + vp + gappoh, m->ww - 2 * c->bw - 2 * sp, m->wh - 2 * c->bw - 2 * vp - 2 * gappoh - th - tabclientgap, 0);
				else
					resize(c, m->wx + sp, m->wy + vp + gappoh + th + tabclientgap, m->ww - 2 * c->bw - 2 * sp, m->wh - 2 * c->bw - 2 * vp - 2 * gappoh - th - tabclientgap, 0);
			} else {
				resize(c, m->wx + sp, m->wy + vp + gappoh, m->ww - 2 * c->bw - 2 * sp, m->wh - 2 * c->bw - 2 * vp - 2 * gappoh, 0);
			}
		}
	}

	if (!topbar) {
		vp = -vp;
	}
}

void
motionnotify(XEvent *e)
{
	static Monitor *mon = NULL;
	Monitor *m;
	XMotionEvent *ev = &e->xmotion;
	unsigned int i, x, occ = 0;
	Client *c;

	m = wintomon(ev->window);
	if (ev->window == selmon->barwin) {
		i = x = 0;
		for (c = m->clients; c; c = c->next) {
			occ |= c->tags == 255 ? 0 : c->tags;
		}
		do {
			/* do not reserve space for vacant tags */
			if (!(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
				continue;
			x += selmon->alttag ? alttagw[i] : tagw[i];
		} while (ev->x >= x && ++i < LENGTH(tags));

	     	if (i < LENGTH(tags)) {
			if ((i + 1) != selmon->previewshow && !(selmon->tagset[selmon->seltags] & 1 << i)) {
	     			selmon->previewshow = i + 1;
	     			showtagpreview(i);
			}
		else if (selmon->tagset[selmon->seltags] & 1 << i) {
				selmon->previewshow = 0;
				showtagpreview(0);
		  }
		} else if (selmon->previewshow != 0) {
	     		selmon->previewshow = 0;
	     		showtagpreview(0);
	     	}
	} else if (selmon->previewshow != 0) {
		selmon->previewshow = 0;
	     	showtagpreview(0);
	}
	if (ev->window != root)
		return;
	if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
		unfocus(selmon->sel, 1);
		if (m != selmon)
			prevmon = selmon;
		selmon = m;
		focus(NULL);
	}
	mon = m;
}

void
moveorplace(const Arg *arg) {
	if ((!selmon->lt[selmon->sellt]->arrange || (selmon->sel && selmon->sel->isfloating)))
		movemouse(arg);
	else
		placemouse(arg);
}

void
movemouse(const Arg *arg)
{
	int x, y, ocx, ocy, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);
			if (abs(selmon->wx - nx) < snap)
				nx = selmon->wx;
			else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
				nx = selmon->wx + selmon->ww - WIDTH(c);
			if (abs(selmon->wy - ny) < snap)
				ny = selmon->wy;
			else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
				ny = selmon->wy + selmon->wh - HEIGHT(c);
			if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
			&& (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
				togglefloating(NULL);
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, c->w, c->h, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		prevmon = selmon;
		selmon = m;
		focus(NULL);
	}
}
void switchtags(const Arg *arg)
{
    unsigned int columns;
    unsigned int new_tagset = 0;
    unsigned int pos, i;
    int col, row;
    Arg new_arg;

    columns = LENGTH(tags) / tagrows + ((LENGTH(tags) % tagrows > 0) ? 1 : 0);

    for (i = 0; i < LENGTH(tags); ++i) {
        if (!(selmon->tagset[selmon->seltags] & 1 << i)) {
            continue;
        }
        pos = i;
        row = pos / columns;
        col = pos % columns;
        if (arg->ui & SWITCHTAG_UP) {     /* UP */
            row --;
            if (row < 0) {
                row = tagrows - 1;
            }
            do {
                pos = row * columns + col;
                row --;
            } while (pos >= LENGTH(tags));
        }
        if (arg->ui & SWITCHTAG_DOWN) {     /* DOWN */
            row ++;
            if (row >= tagrows) {
                row = 0;
            }
            pos = row * columns + col;
            if (pos >= LENGTH(tags)) {
                row = 0;
            }
            pos = row * columns + col;
        }
        if (arg->ui & SWITCHTAG_LEFT) {     /* LEFT */
            col --;
            if (col < 0) {
                col = columns - 1;
            }
            do {
                pos = row * columns + col;
                col --;
            } while (pos >= LENGTH(tags));
        }
        if (arg->ui & SWITCHTAG_RIGHT) {     /* RIGHT */
            col ++;
            if (col >= columns) {
                col = 0;
            }
            pos = row * columns + col;
            if (pos >= LENGTH(tags)) {
                col = 0;
                pos = row * columns + col;
            }
        }
        new_tagset |= 1 << pos;
    }
    new_arg.ui = new_tagset;
    if (arg->ui & SWITCHTAG_TOGGLETAG) {
        toggletag(&new_arg);
    }
    if (arg->ui & SWITCHTAG_TAG) {
        tag(&new_arg);
    }
    if (arg->ui & SWITCHTAG_VIEW) {
        view (&new_arg);
    }
    if (arg->ui & SWITCHTAG_TOGGLEVIEW) {
        toggleview (&new_arg);
    }
}

void
placemouse(const Arg *arg)
{
	int x, y, px, py, ocx, ocy, nx = -9999, ny = -9999, freemove = 0;
	Client *c, *r = NULL, *at, *prevr;
	Monitor *m;
	XEvent ev;
	XWindowAttributes wa;
	Time lasttime = 0;
	int attachmode, prevattachmode;
	attachmode = prevattachmode = -1;

	if (!(c = selmon->sel) || !c->mon->lt[c->mon->sellt]->arrange) /* no support for placemouse when floating layout is used */
		return;
	if (c->isfullscreen) /* no support placing fullscreen windows by mouse */
		return;
	restack(selmon);
	prevr = c;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;

	c->isfloating = 0;
	c->beingmoved = 1;

	XGetWindowAttributes(dpy, c->win, &wa);
	ocx = wa.x;
	ocy = wa.y;

	if (arg->i == 2) // warp cursor to client center
		XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, WIDTH(c) / 2, HEIGHT(c) / 2);

	if (!getrootptr(&x, &y))
		return;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch (ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nx = ocx + (ev.xmotion.x - x);
			ny = ocy + (ev.xmotion.y - y);

			if (!freemove && (abs(nx - ocx) > snap || abs(ny - ocy) > snap))
				freemove = 1;

			if (freemove)
				XMoveWindow(dpy, c->win, nx, ny);

			if ((m = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) && m != selmon)
				selmon = m;

			if (arg->i == 1) { // tiled position is relative to the client window center point
				px = nx + wa.width / 2;
				py = ny + wa.height / 2;
			} else { // tiled position is relative to the mouse cursor
				px = ev.xmotion.x;
				py = ev.xmotion.y;
			}

			r = recttoclient(px, py, 1, 1);

			if (!r || r == c)
				break;

			attachmode = 0; // below
			if (((float)(r->y + r->h - py) / r->h) > ((float)(r->x + r->w - px) / r->w)) {
				if (abs(r->y - py) < r->h / 2)
					attachmode = 1; // above
			} else if (abs(r->x - px) < r->w / 2)
				attachmode = 1; // above

			if ((r && r != prevr) || (attachmode != prevattachmode)) {
				detachstack(c);
				detach(c);
				if (c->mon != r->mon) {
					arrangemon(c->mon);
					c->tags = r->mon->tagset[r->mon->seltags];
				}

				c->mon = r->mon;
				r->mon->sel = r;

				if (attachmode) {
					if (r == r->mon->clients)
						attach(c);
					else {
						for (at = r->mon->clients; at->next != r; at = at->next);
						c->next = at->next;
						at->next = c;
					}
				} else {
					c->next = r->next;
					r->next = c;
				}

				attachstack(c);
				arrangemon(r->mon);
				prevr = r;
				prevattachmode = attachmode;
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);

	if ((m = recttomon(ev.xmotion.x, ev.xmotion.y, 1, 1)) && m != c->mon) {
		detach(c);
		detachstack(c);
		arrangemon(c->mon);
		c->mon = m;
		c->tags = m->tagset[m->seltags];
		attach(c);
		attachstack(c);
		selmon = m;
	}

	focus(c);
	c->beingmoved = 0;

	if (nx != -9999)
		resize(c, nx, ny, c->w, c->h, 0);
	arrangemon(c->mon);
}

void
moveresize(const Arg *arg) {
	/* only floating windows can be moved */
	Client *c;
	c = selmon->sel;
	int x, y, w, h, nx, ny, nw, nh, ox, oy, ow, oh;
	char xAbs, yAbs, wAbs, hAbs;
	int msx, msy, dx, dy, nmx, nmy;
	unsigned int dui;
	Window dummy;

	if (!c || !arg)
		return;
	if (!(c && arg && arg->v))
		return;
	if (selmon->lt[selmon->sellt]->arrange && !c->isfloating)
		togglefloating(NULL);
	if (sscanf((char *)arg->v, "%d%c %d%c %d%c %d%c", &x, &xAbs, &y, &yAbs, &w, &wAbs, &h, &hAbs) != 8)
		return;

	/* compute new window position; prevent window from be positioned outside the current monitor */
	nw = c->w + w;
	if (wAbs == 'W')
		nw = w < selmon->mw - 2 * c->bw ? w : selmon->mw - 2 * c->bw;

	nh = c->h + h;
	if (hAbs == 'H')
		nh = h < selmon->mh - 2 * c->bw ? h : selmon->mh - 2 * c->bw;

	nx = c->x + x;
	if (xAbs == 'X') {
		if (x < selmon->mx)
			nx = selmon->mx;
		else if (x > selmon->mx + selmon->mw)
			nx = selmon->mx + selmon->mw - nw - 2 * c->bw;
		else
			nx = x;
	}

	ny = c->y + y;
	if (yAbs == 'Y') {
		if (y < selmon->my)
			ny = selmon->my;
		else if (y > selmon->my + selmon->mh)
			ny = selmon->my + selmon->mh - nh - 2 * c->bw;
		else
			ny = y;
	}

	ox = c->x;
	oy = c->y;
	ow = c->w;
	oh = c->h;

	XRaiseWindow(dpy, c->win);
	Bool xqp = XQueryPointer(dpy, root, &dummy, &dummy, &msx, &msy, &dx, &dy, &dui);
	resize(c, nx, ny, nw, nh, True);

	/* move cursor along with the window to avoid problems caused by the sloppy focus */
	if (xqp && ox <= msx && (ox + ow) >= msx && oy <= msy && (oy + oh) >= msy)
	{
		nmx = c->x - ox + c->w - ow;
		nmy = c->y - oy + c->h - oh;
		/* make sure the cursor stays inside the window */
		if ((msx + nmx) > c->x && (msy + nmy) > c->y)
			XWarpPointer(dpy, None, None, 0, 0, 0, 0, nmx, nmy);
	}
}

void
moveresizeedge(const Arg *arg) {
	/* move or resize floating window to edge of screen */
	Client *c;
	c = selmon->sel;
	char e;
	int nx, ny, nw, nh, ox, oy, ow, oh, bp;
	int msx, msy, dx, dy, nmx, nmy;
	int starty;
	unsigned int dui;
	Window dummy;

	nx = c->x;
	ny = c->y;
	nw = c->w;
	nh = c->h;

	starty = selmon->showbar && topbar ? bh : 0;
	bp = selmon->showbar && !topbar ? bh : 0;

	if (!c || !arg)
		return;
	if (selmon->lt[selmon->sellt]->arrange && !c->isfloating)
		return;
	if(sscanf((char *)arg->v, "%c", &e) != 1)
		return;

	if(e == 't')
		ny = starty;

	if(e == 'b')
		ny = c->h > selmon->mh - 2 * c->bw ? c->h - bp : selmon->mh - c->h - 2 * c->bw - bp;

	if(e == 'l')
		nx = selmon->mx;

	if(e == 'r')
		nx = c->w > selmon->mw - 2 * c->bw ? selmon->mx + c->w : selmon->mx + selmon->mw - c->w - 2 * c->bw;

	if(e == 'T') {
		/* if you click to resize again, it will return to old size/position */
		if(c->h + starty == c->oldh + c->oldy) {
			nh = c->oldh;
			ny = c->oldy;
		} else {
			nh = c->h + c->y - starty;
			ny = starty;
		}
	}

	if(e == 'B')
		nh = c->h + c->y + 2 * c->bw + bp == selmon->mh ? c->oldh : selmon->mh - c->y - 2 * c->bw - bp;

	if(e == 'L') {
		if(selmon->mx + c->w == c->oldw + c->oldx) {
			nw = c->oldw;
			nx = c->oldx;
		} else {
			nw = c->w + c->x - selmon->mx;
			nx = selmon->mx;
		}
	}

	if(e == 'R')
		nw = c->w + c->x + 2 * c->bw == selmon->mx + selmon->mw ? c->oldw : selmon->mx + selmon->mw - c->x - 2 * c->bw;

	ox = c->x;
	oy = c->y;
	ow = c->w;
	oh = c->h;

	XRaiseWindow(dpy, c->win);
	Bool xqp = XQueryPointer(dpy, root, &dummy, &dummy, &msx, &msy, &dx, &dy, &dui);
	resize(c, nx, ny, nw, nh, True);

	/* move cursor along with the window to avoid problems caused by the sloppy focus */
	if (xqp && ox <= msx && (ox + ow) >= msx && oy <= msy && (oy + oh) >= msy) {
		nmx = c->x - ox + c->w - ow;
		nmy = c->y - oy + c->h - oh;
		/* make sure the cursor stays inside the window */
		if ((msx + nmx) > c->x && (msy + nmy) > c->y)
			XWarpPointer(dpy, None, None, 0, 0, 0, 0, nmx, nmy);
	}
}

Client *
nexttiled(Client *c)
{
	for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next);
	return c;
}

void
opacity(Client *c, double opacity)
{
	if(opacity > 0 && opacity < 1) {
		unsigned long real_opacity[] = { opacity * 0xffffffff };
		XChangeProperty(dpy, c->win, netatom[NetWMWindowsOpacity], XA_CARDINAL,
				32, PropModeReplace, (unsigned char *)real_opacity,
				1);
	} else
		XDeleteProperty(dpy, c->win, netatom[NetWMWindowsOpacity]);
}

/*
void
pop(Client *c)
{
	int i;
	for (i = 0; !(selmon->tagset[selmon->seltags] & 1 << i); i++);
	i++;

	c->mon->tagmarked[i] = nexttiled(c->mon->clients);
	detach(c);
	attach(c);
	focus(c);
	arrange(c->mon);
}
*/

Client *
prevtiled(Client *c) {
	Client *p, *r;

	for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
		if(!p->isfloating && ISVISIBLE(p))
			r = p;
	return r;
}

void
propertynotify(XEvent *e)
{
	Client *c;
	Window trans;
	Swallow *s;
	XPropertyEvent *ev = &e->xproperty;

	if ((c = wintosystrayicon(ev->window))) {
		if (ev->atom == XA_WM_NORMAL_HINTS) {
			updatesizehints(c);
			updatesystrayicongeom(c, c->w, c->h);
		}
		else
			updatesystrayiconstate(c, ev);
		resizebarwin(selmon);
		updatesystray();
	}
	if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
		if (!fakesignal())
			updatestatus();
		if (!fake_signal())
			updatestatus();
	} else if (ev->state == PropertyDelete)
		return; /* ignore */
	else if ((c = wintoclient(ev->window))) {
		switch(ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
				(c->isfloating = (wintoclient(trans)) != NULL))
				arrange(c->mon);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbars();
			drawtabs();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
			updatetitle(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
			drawtab(c->mon);
			if (swalretroactive && (s = swalmatch(c->win))) {
				swal(s->client, c, 0);
			}
		}
		else if (ev->atom == netatom[NetWMIcon]) {
			updateicon(c);
			if (c == c->mon->sel)
				drawbar(c->mon);
		}
		if (ev->atom == netatom[NetWMWindowType])
			updatewindowtype(c);
		if (ev->atom == motifatom)
			updatemotifhints(c);
	}
}

void
pushdown(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating || sel == nexttiled(selmon->clients))
		return;
	if((c = nexttiled(sel->next))) {
		detach(sel);
		sel->next = c->next;
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

void
pushup(const Arg *arg) {
	Client *sel = selmon->sel, *c;

	if(!sel || sel->isfloating)
		return;
	if((c = prevtiled(sel)) && c != nexttiled(selmon->clients)) {
		detach(sel);
		sel->next = c;
		for(c = selmon->clients; c->next != sel->next; c = c->next);
		c->next = sel;
	}
	focus(sel);
	arrange(selmon);
}

void
quit(const Arg *arg)
{
	// fix: reloading dwm keeps all the hidden clients hidden
	Monitor *m;
	Client *c;
	for (m = mons; m; m = m->next) {
		if (m) {
			for (c = m->stack; c; c = c->next)
				if (c && HIDDEN(c)) showwin(c);
		}
	}

	if(arg->i) restart = 1;
	running = 0;
}

Client *
recttoclient(int x, int y, int w, int h)
{
	Client *c, *r = NULL;
	int a, area = 0;

	for (c = nexttiled(selmon->clients); c; c = nexttiled(c->next)) {
		if ((a = INTERSECTC(x, y, w, h, c)) > area) {
			area = a;
			r = c;
		}
	}
	return r;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
	Monitor *m, *r = selmon;
	int a, area = 0;

	for (m = mons; m; m = m->next)
		if ((a = INTERSECT(x, y, w, h, m)) > area) {
			area = a;
			r = m;
		}
	return r;
}

void
removesystrayicon(Client *i)
{
	Client **ii;

	if (!showsystray || !i)
		return;
	for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
	if (ii)
		*ii = i->next;
	free(i);
}


void
resize(Client *c, int x, int y, int w, int h, int interact)
{
	if (applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizebarwin(Monitor *m) {
	unsigned int w = m->ww;
	if (showsystray && m == systraytomon(m) && !systrayonleft)
		w -= getsystraywidth();
	XMoveResizeWindow(dpy, m->barwin, m->wx + sp, m->by + vp, w - 2 * sp, bh);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	if (c->beingmoved)
		return;
	if (c->isfloating && c->hasfloatbw && !c->isfullscreen)
		wc.border_width = c->floatborderpx;
	else
		wc.border_width = c->bw;
	if (((nexttiled(c->mon->clients) == c && !nexttiled(c->next))
	    || &monocle == c->mon->lt[c->mon->sellt]->arrange)
	    && !c->isfullscreen && !c->isfloating
	    && NULL != c->mon->lt[c->mon->sellt]->arrange) {
		c->w = wc.width += c->bw * 2;
		c->h = wc.height += c->bw * 2;
		wc.border_width = 0;
	}
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
	int ocx, ocy, nw, nh;
	int ocx2, ocy2, nx, ny;
	Client *c;
	Monitor *m;
	XEvent ev;
	int horizcorner, vertcorner;
	int di;
	unsigned int dui;
	Window dummy;
	Time lasttime = 0;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	ocx = c->x;
	ocy = c->y;
	ocx2 = c->x + c->w;
	ocy2 = c->y + c->h;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!XQueryPointer (dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny, &dui))
		return;
	horizcorner = nx < c->w / 2;
	vertcorner  = ny < c->h / 2;
	XWarpPointer (dpy, None, c->win, 0, 0, 0, 0,
			horizcorner ? (-c->bw) : (c->w + c->bw -1),
			vertcorner  ? (-c->bw) : (c->h + c->bw -1));
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;

			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
			nx = horizcorner ? ev.xmotion.x : c->x;
			ny = vertcorner ? ev.xmotion.y : c->y;
			nw = MAX(horizcorner ? (ocx2 - nx) : (ev.xmotion.x - ocx - 2 * c->bw + 1), 1);
			nh = MAX(vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

			if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
			&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
			{
				if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
				&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
					togglefloating(NULL);
			}
			if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
				resize(c, nx, ny, nw, nh, 1);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
		      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
		      vertcorner ? (-c->bw) : (c->h + c->bw - 1));
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		prevmon = selmon;
		selmon = m;
		focus(NULL);
	}
}

void
resizerequest(XEvent *e)
{
	XResizeRequestEvent *ev = &e->xresizerequest;
	Client *i;

	if ((i = wintosystrayicon(ev->window))) {
		updatesystrayicongeom(i, ev->width, ev->height);
		resizebarwin(selmon);
		updatesystray();
	}
}

void
resizemousescroll(const Arg *arg)
{
	int nw, nh;
	Client *c;
	Monitor *m;
	XEvent ev;
	int dw = *((int*)arg->v + 1);
	int dh = *(int*)arg->v;

	if (!(c = selmon->sel))
		return;
	if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
		return;
	restack(selmon);
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
		return;
	nw = MAX(c->w + dw, 1);
	nh = MAX(c->h + dh, 1);
	if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
	&& c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
	{
		if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
		&& (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
			togglefloating(NULL);
	}
	if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
		resize(c, c->x, c->y, nw, nh, 1);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
	if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
		sendmon(c, m);
		selmon = m;
		focus(NULL);
	}
}

void
restack(Monitor *m)
{
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar(m);
	drawtab(m);
	if (!m->sel)
		return;
	if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
		XRaiseWindow(dpy, m->sel->win);

	/* raise the aot window */
	for(Monitor *m_search = mons; m_search; m_search = m_search->next){
		for(c = m_search->clients; c; c = c->next){
			if(c->isalwaysontop){
				XRaiseWindow(dpy, c->win);
				break;
			}
		}
	}

	if (m->lt[m->sellt]->arrange) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for (c = m->stack; c; c = c->snext)
			if (!c->isfloating && ISVISIBLE(c)) {
				XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
				wc.sibling = c->win;
			}
	}
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

int
riodraw(Client *c, const char slopstyle[])
{
	int i;
	char str[100] = {0};
	char strout[100] = {0};
	char tmpstring[30] = {0};
	char slopcmd[100] = "slop -f x%xx%yx%wx%hx ";
	int firstchar = 0;
	int counter = 0;

	strcat(slopcmd, slopstyle);
	FILE *fp = popen(slopcmd, "r");

	while (fgets(str, 100, fp) != NULL)
		strcat(strout, str);

	pclose(fp);

	if (strlen(strout) < 6)
		return 0;

	for (i = 0; i < strlen(strout); i++){
		if (!firstchar) {
			if (strout[i] == 'x')
				firstchar = 1;
			continue;
		}

		if (strout[i] != 'x')
			tmpstring[strlen(tmpstring)] = strout[i];
		else {
			riodimensions[counter] = atoi(tmpstring);
			counter++;
			memset(tmpstring,0,strlen(tmpstring));
		}
	}

	if (riodimensions[0] <= -40 || riodimensions[1] <= -40 || riodimensions[2] <= 50 || riodimensions[3] <= 50) {
		riodimensions[3] = -1;
		return 0;
	}

	if (c) {
		rioposition(c, riodimensions[0], riodimensions[1], riodimensions[2], riodimensions[3]);
		return 0;
	}

	return 1;
}

void
rioposition(Client *c, int x, int y, int w, int h)
{
	Monitor *m;
	if ((m = recttomon(x, y, w, h)) && m != c->mon) {
		detach(c);
		detachstack(c);
		arrange(c->mon);
		c->mon = m;
		c->tags = m->tagset[m->seltags];
		attach(c);
		attachstack(c);
		selmon = m;
		focus(c);
	}

	c->isfloating = 1;
	if (riodraw_borders)
		resizeclient(c, x, y, w - (c->bw * 2), h - (c->bw * 2));
	else
		resizeclient(c, x - c->bw, y - c->bw, w, h);
	arrange(c->mon);

	riodimensions[3] = -1;
	riopid = 0;
}

/* drag out an area using slop and resize the selected window to it */
void
rioresize(const Arg *arg)
{
	Client *c = (arg && arg->v ? (Client*)arg->v : selmon->sel);
	if (c)
		riodraw(c, slopresizestyle);
}

/* spawn a new window and drag out an area using slop to position it */
void
riospawn(const Arg *arg)
{
	if (riodraw_spawnasync) {
		riopid = spawncmd(arg);
		riodraw(NULL, slopspawnstyle);
	} else if (riodraw(NULL, slopspawnstyle))
		riopid = spawncmd(arg);
}

void
run(void)
{
	int event_count = 0;
	const int MAX_EVENTS = 10;
	struct epoll_event events[MAX_EVENTS];

	XSync(dpy, False);

	/* main event loop */
	while (running) {
		event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

		for (int i = 0; i < event_count; i++) {
			int event_fd = events[i].data.fd;
			DEBUG("Got event from fd %d\n", event_fd);

			if (event_fd == dpy_fd) {
				// -1 means EPOLLHUP
				if (handlexevent(events + i) == -1)
					return;
			} else if (event_fd == ipc_get_sock_fd()) {
				ipc_handle_socket_epoll_event(events + i);
			} else if (ipc_is_client_registered(event_fd)){
				if (ipc_handle_client_epoll_event(events + i, mons, &lastselmon, selmon,
							tags, LENGTH(tags), layouts, LENGTH(layouts)) < 0) {
					fprintf(stderr, "Error handling IPC event on fd %d\n", event_fd);
				}
			} else {
				fprintf(stderr, "Got event from unknown fd %d, ptr %p, u32 %d, u64 %lu",
						event_fd, events[i].data.ptr, events[i].data.u32,
						events[i].data.u64);
				fprintf(stderr, " with events %d\n", events[i].events);
				return;
			}
		}
	}
}

void
runautostart(void)
{
	char *pathpfx;
	char *path;
	char *xdgdatahome;
	char *home;
	struct stat sb;

	if ((home = getenv("HOME")) == NULL)
		/* this is almost impossible */
		return;

	/* if $XDG_DATA_HOME is set and not empty, use $XDG_DATA_HOME/dwm,
	 * otherwise use ~/.local/share/dwm as autostart script directory
	 */
	xdgdatahome = getenv("XDG_DATA_HOME");
	if (xdgdatahome != NULL && *xdgdatahome != '\0') {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(xdgdatahome) + strlen(dwmdir) + 2);

		if (sprintf(pathpfx, "%s/%s", xdgdatahome, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	} else {
		/* space for path segments, separators and nul */
		pathpfx = ecalloc(1, strlen(home) + strlen(localshare)
		                     + strlen(dwmdir) + 3);

		if (sprintf(pathpfx, "%s/%s/%s", home, localshare, dwmdir) < 0) {
			free(pathpfx);
			return;
		}
	}

	/* check if the autostart script directory exists */
	if (! (stat(pathpfx, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		/* the XDG conformant path does not exist or is no directory
		 * so we try ~/.dwm instead
		 */
		char *pathpfx_new = realloc(pathpfx, strlen(home) + strlen(dwmdir) + 3);
		if(pathpfx_new == NULL) {
			free(pathpfx);
			return;
		}
		pathpfx = pathpfx_new;

		if (sprintf(pathpfx, "%s/.%s", home, dwmdir) <= 0) {
			free(pathpfx);
			return;
		}
	}

	/* try the blocking script first */
	path = ecalloc(1, strlen(pathpfx) + strlen(autostartblocksh) + 2);
	if (sprintf(path, "%s/%s", pathpfx, autostartblocksh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(path);

	/* now the non-blocking script */
	if (sprintf(path, "%s/%s", pathpfx, autostartsh) <= 0) {
		free(path);
		free(pathpfx);
	}

	if (access(path, X_OK) == 0)
		system(strcat(path, " &"));

	free(pathpfx);
	free(path);
}

void
scan(void)
{
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) { /* now the transients */
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
}

static void scratchpad_hide ()
{
	if (selmon -> sel)
	{
		selmon -> sel -> tags = SCRATCHPAD_MASK;
		selmon -> sel -> isfloating = 1;
		focus(NULL);
		arrange(selmon);
	}
}

static _Bool scratchpad_last_showed_is_killed (void)
{
	_Bool killed = 1;
	for (Client * c = selmon -> clients; c != NULL; c = c -> next)
	{
		if (c == scratchpad_last_showed)
		{
			killed = 0;
			break;
		}
	}
	return killed;
}

static void scratchpad_remove ()
{
	if (selmon -> sel && scratchpad_last_showed != NULL && selmon -> sel == scratchpad_last_showed)
		scratchpad_last_showed = NULL;
}

static void scratchpad_show ()
{
	if (scratchpad_last_showed == NULL || scratchpad_last_showed_is_killed ())
		scratchpad_show_first ();
	else
	{
		if (scratchpad_last_showed -> tags != SCRATCHPAD_MASK)
		{
			scratchpad_last_showed -> tags = SCRATCHPAD_MASK;
			focus(NULL);
			arrange(selmon);
		}
		else
		{
			_Bool found_current = 0;
			_Bool found_next = 0;
			for (Client * c = selmon -> clients; c != NULL; c = c -> next)
			{
				if (found_current == 0)
				{
					if (c == scratchpad_last_showed)
					{
						found_current = 1;
						continue;
					}
				}
				else
				{
					if (c -> tags == SCRATCHPAD_MASK)
					{
						found_next = 1;
						scratchpad_show_client (c);
						break;
					}
				}
			}
			if (found_next == 0) scratchpad_show_first ();
		}
	}
}

static void scratchpad_show_client (Client * c)
{
	scratchpad_last_showed = c;
	c -> tags = selmon->tagset[selmon->seltags];
	focus(c);
	arrange(selmon);
}

static void scratchpad_show_first (void)
{
	for (Client * c = selmon -> clients; c != NULL; c = c -> next)
	{
		if (c -> tags == SCRATCHPAD_MASK)
		{
			scratchpad_show_client (c);
			break;
		}
	}
}

void
sendmon(Client *c, Monitor *m)
{
	if (c->mon == m)
		return;
	unfocus(c, 1);
	detach(c);
	detachstack(c);
	c->mon = m;
	c->tags = (m->tagset[m->seltags] ? m->tagset[m->seltags] : 1);
	attachx(c);
	attachstack(c);
	focus(NULL);
	arrange(NULL);
	if (c->switchtag)
		c->switchtag = 0;
}

void
setborderpx(const Arg *arg)
{
	Client *c;
	int prev_borderpx = selmon->borderpx;

	if (arg->i == 0)
		selmon->borderpx = borderpx;
	else if (selmon->borderpx + arg->i < 0)
		selmon->borderpx = 0;	
	else
		selmon->borderpx += arg->i;	
	
	for (c = selmon->clients; c; c = c->next)
	{	
		if (c->bw + arg->i < 0)
			c->bw = selmon->borderpx = 0;
		else
			c->bw = selmon->borderpx;
		if (c->isfloating || !selmon->lt[selmon->sellt]->arrange)
		{
			if (arg->i != 0 && prev_borderpx + arg->i >= 0)
				resize(c, c->x, c->y, c->w-(arg->i*2), c->h-(arg->i*2), 0);
			else if (arg->i != 0)
				resizeclient(c, c->x, c->y, c->w, c->h);
			else if (prev_borderpx > borderpx)
				resize(c, c->x, c->y, c->w + 2*(prev_borderpx - borderpx), c->h + 2*(prev_borderpx - borderpx), 0);
			else if (prev_borderpx < borderpx)
				resize(c, c->x, c->y, c->w-2*(borderpx - prev_borderpx), c->h-2*(borderpx - prev_borderpx), 0);
		}
	}
	arrange(selmon);
}

void
setclientstate(Client *c, long state)
{
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
		PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
	int n;
	Atom *protocols, mt;
	int exists = 0;
	XEvent ev;

	if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
		mt = wmatom[WMProtocols];
		if (XGetWMProtocols(dpy, w, &protocols, &n)) {
			while (!exists && n--)
				exists = protocols[n] == proto;
			XFree(protocols);
		}
	}
	else {
		exists = True;
		mt = proto;
	}
	if (exists) {
		ev.type = ClientMessage;
		ev.xclient.window = w;
		ev.xclient.message_type = mt;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = d0;
		ev.xclient.data.l[1] = d1;
		ev.xclient.data.l[2] = d2;
		ev.xclient.data.l[3] = d3;
		ev.xclient.data.l[4] = d4;
		XSendEvent(dpy, w, False, mask, &ev);
	}
	return exists;
}

void
setfocus(Client *c)
{
	if (!c->neverfocus) {
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XChangeProperty(dpy, root, netatom[NetActiveWindow],
			XA_WINDOW, 32, PropModeReplace,
			(unsigned char *) &(c->win), 1);
	}
	sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

void
setfullscreen(Client *c, int fullscreen)
{
	if (fullscreen && !c->isfullscreen) {
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
		c->isfullscreen = 1;
		c->oldstate = c->isfloating;
		c->oldbw = c->bw;
		c->bw = 0;
		c->isfloating = 1;
		resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
		XRaiseWindow(dpy, c->win);
	} else if (!fullscreen && c->isfullscreen){
		XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
			PropModeReplace, (unsigned char*)0, 0);
		c->isfullscreen = 0;
		c->isfloating = c->oldstate;
		c->bw = c->oldbw;
		c->x = c->oldx;
		c->y = c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		resizeclient(c, c->x, c->y, c->w, c->h);
		arrange(c->mon);
	}
}

void
setlayout(const Arg *arg)
{
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
	if (selmon->sel)
		arrange(selmon);
	else
		drawbar(selmon);
}

void
setcfact(const Arg *arg)
{
	float f;
	Client *c;

	c = selmon->sel;

	if (!arg || !c || !selmon->lt[selmon->sellt]->arrange)
		return;
	if (!arg->f)
		f = 1.0;
	else if (arg->f > 4.0) // set fact absolutely
		f = arg->f - 4.0;
	else
		f = arg->f + c->cfact;
	if (f < 0.25)
		f = 0.25;
	else if (f > 4.0)
		f = 4.0;
	c->cfact = f;
	arrange(selmon);
}

void
setlayoutsafe(const Arg *arg)
{
	const Layout *ltptr = (Layout *)arg->v;
	if (ltptr == 0)
			setlayout(arg);
	for (int i = 0; i < LENGTH(layouts); i++) {
		if (ltptr == &layouts[i])
			setlayout(arg);
	}
}

void
setmark(Client *c)
{
	if (c == mark)
		return;
	if (mark) {
		XSetWindowBorder(dpy, mark->win, scheme[mark == selmon->sel
				? SchemeSel : SchemeNorm][ColBorder].pixel);
		mark = 0;
	}
	if (c) {
		XSetWindowBorder(dpy, c->win, scheme[c == selmon->sel
				? SchemeSel : SchemeNorm][ColMark].pixel);
		mark = c;
	}
}

/* arg > 1.0 will set mfact absolutely */
void
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.05 || f > 0.95)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void
setup(void)
{
	int i;
	XSetWindowAttributes wa;
	Atom utf8string;

	/* clean up any zombies immediately */
	sigchld(0);

	signal(SIGHUP, sighup);
	signal(SIGTERM, sigterm);

	/* init screen */
	screen = DefaultScreen(dpy);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	root = RootWindow(dpy, screen);
	xinitvisual();
	drw = drw_create(dpy, screen, root, sw, sh, visual, depth, cmap);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h + horizpadbar;
	bh = user_bh ? user_bh : drw->fonts->h + horizpadbar;
	th = bh;
	updategeom();
	sp = sidepad;
	vp = (topbar == 1) ? vertpad : - vertpad;

	/* init atoms */
	utf8string = XInternAtom(dpy, "UTF8_STRING", False);
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	wmatom[WMWindowRole] = XInternAtom(dpy, "WM_WINDOW_ROLE", False);
	netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
	netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
	netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMIcon] = XInternAtom(dpy, "_NET_WM_ICON", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	motifatom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
	netatom[NetWMWindowsOpacity] = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
	xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
	xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
	/* init cursors */
	cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	cursor[CurResize] = drw_cur_create(drw, XC_sizing);
	cursor[CurMove] = drw_cur_create(drw, XC_fleur);
	cursor[CurSwal] = drw_cur_create(drw, XC_bottom_side);
	cursor[CurResizeHorzArrow] = drw_cur_create(drw, XC_sb_h_double_arrow);
	cursor[CurResizeVertArrow] = drw_cur_create(drw, XC_sb_v_double_arrow);
	/* init appearance */
	if (LENGTH(tags) > LENGTH(tagsel))
		die("too few color schemes for the tags");
	scheme = ecalloc(LENGTH(colors) + 1, sizeof(Clr *));
	scheme[LENGTH(colors)] = drw_scm_create(drw, colors[0], alphas[0], 4);
	for (i = 0; i < LENGTH(colors); i++)
		scheme[i] = drw_scm_create(drw, colors[i], alphas[i], 4);
	tagscheme = ecalloc(LENGTH(tagsel), sizeof(Clr *));
	for (i = 0; i < LENGTH(tagsel); i++)
		tagscheme[i] = drw_scm_create(drw, tagsel[i], tagalpha, 2);
	/* init system tray */
	updatesystray();
	/* init bars */
	updatebars();
	updatestatus();
	updatebarpos(selmon);
	updatepreview();
	/* supporting window for NetWMCheck */
	wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
		PropModeReplace, (unsigned char *) "dwm", 3);
	XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
		PropModeReplace, (unsigned char *) &wmcheckwin, 1);
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
		PropModeReplace, (unsigned char *) netatom, NetLast);
	XDeleteProperty(dpy, root, netatom[NetClientList]);
	/* select events */
	wa.cursor = cursor[CurNormal]->cursor;
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
		|ButtonPressMask|PointerMotionMask|EnterWindowMask
		|LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
	focus(NULL);
	setupepoll();
}

void
setupepoll(void)
{
	epoll_fd = epoll_create1(0);
	dpy_fd = ConnectionNumber(dpy);
	struct epoll_event dpy_event;

	// Initialize struct to 0
	memset(&dpy_event, 0, sizeof(dpy_event));

	DEBUG("Display socket is fd %d\n", dpy_fd);

	if (epoll_fd == -1) {
		fputs("Failed to create epoll file descriptor", stderr);
	}

	dpy_event.events = EPOLLIN;
	dpy_event.data.fd = dpy_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, dpy_fd, &dpy_event)) {
		fputs("Failed to add display file descriptor to epoll", stderr);
		close(epoll_fd);
		exit(1);
	}

	if (ipc_init(ipcsockpath, epoll_fd, ipccommands, LENGTH(ipccommands)) < 0) {
		fputs("Failed to initialize IPC\n", stderr);
	}
}

void
seturgent(Client *c, int urg)
{
	XWMHints *wmh;

	c->isurgent = urg;
	if (!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
show(const Arg *arg)
{
	if (selmon->hidsel)
		selmon->hidsel = 0;
	showwin(selmon->sel);
}

void
showwin(Client *c)
{
	if (!c || !HIDDEN(c))
		return;

	XMapWindow(dpy, c->win);
	setclientstate(c, NormalState);
	arrange(c->mon);
}

void
showhide(Client *c)
{
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		/* show clients top down */
		XMoveWindow(dpy, c->win, c->x, c->y);
		if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
			resize(c, c->x, c->y, c->w, c->h, 0);
		showhide(c->snext);
	} else {
		/* hide clients bottom up */
		showhide(c->snext);
		XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
	}
}

void
showtagpreview(int tag)
{
	if (!selmon->previewshow) {
		XUnmapWindow(dpy, selmon->tagwin);
		return;
	}

        if (selmon->tagmap[tag]) {
		XSetWindowBackgroundPixmap(dpy, selmon->tagwin, selmon->tagmap[tag]);
		XCopyArea(dpy, selmon->tagmap[tag], selmon->tagwin, XCreateGC(dpy, root, 0, NULL), 0, 0, selmon->mw / scalepreview, selmon->mh / scalepreview, 0, 0);
		XSync(dpy, False);
		XMapWindow(dpy, selmon->tagwin);
	} else
		XUnmapWindow(dpy, selmon->tagwin);
}

void
sigchld(int unused)
{
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		die("can't install SIGCHLD handler:");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
sighup(int unused)
{
	Arg a = {.i = 1};
	quit(&a);
}

void
sigterm(int unused)
{
	Arg a = {.i = 0};
	quit(&a);
}

void
sigstatusbar(const Arg *arg)
{
	union sigval sv;

	if (!statussig)
		return;
	sv.sival_int = arg->i;
	if ((statuspid = getstatusbarpid()) <= 0)
		return;

	sigqueue(statuspid, SIGRTMIN+statussig, sv);
}

void
spawn(const Arg *arg)
{
	spawncmd(arg);
}

pid_t
spawncmd(const Arg *arg)
{
	pid_t pid;
	if (arg->v == dmenucmd)
		dmenumon[0] = '0' + selmon->num;
	if ((pid = fork()) == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
	return pid;
}

void
switchtag(void)
{
  	int i;
	unsigned int occ = 0;
	Client *c;
	Imlib_Image image;

	for (c = selmon->clients; c; c = c->next)
		occ |= c->tags == 255 ? 0 : c->tags;
	for (i = 0; i < LENGTH(tags); i++) {
		if (selmon->tagset[selmon->seltags] & 1 << i) {
                  	if (selmon->tagmap[i] != 0) {
 				XFreePixmap(dpy, selmon->tagmap[i]);
 				selmon->tagmap[i] = 0;
 			}
			if (occ & 1 << i) {
				image = imlib_create_image(sw, sh);
				imlib_context_set_image(image);
				imlib_context_set_display(dpy);
				imlib_context_set_visual(DefaultVisual(dpy, screen));
				imlib_context_set_drawable(RootWindow(dpy, screen));
				imlib_copy_drawable_to_image(0, selmon->mx, selmon->my, selmon->mw ,selmon->mh, 0, 0, 1);
				selmon->tagmap[i] = XCreatePixmap(dpy, selmon->tagwin, selmon->mw / scalepreview, selmon->mh / scalepreview, DefaultDepth(dpy, screen));
				imlib_context_set_drawable(selmon->tagmap[i]);
				imlib_render_image_part_on_drawable_at_size(0, 0, selmon->mw, selmon->mh, 0, 0, selmon->mw / scalepreview, selmon->mh / scalepreview);
				imlib_free_image();
			}
		}
	}
}

/*
 * Perform immediate swallow of client 'swee' by client 'swer'. 'manage' shall
 * be set if swal() is called from swalmanage(). 'swer' and 'swee' must be
 * regular or swallowee, but not swallower.
 */
void
swal(Client *swer, Client *swee, int manage)
{
	Client *c, **pc;
	int sweefocused = selmon->sel == swee;

	/* Remove any swallows registered for the swer. Asking a swallower to
	 * swallow another window is ambiguous and is thus avoided altogether. In
	 * contrast, a swallowee can swallow in a well-defined manner by attaching
	 * to the head of the swallow chain. */
	if (!manage)
		swalunreg(swer);

	/* Disable fullscreen prior to swallow. Swallows involving fullscreen
	 * windows produces quirky artefacts such as fullscreen terminals or tiled
	 * pseudo-fullscreen windows. */
	setfullscreen(swer, 0);
	setfullscreen(swee, 0);

	/* Swap swallowee into client and focus lists. Keeps current focus unless
	 * the swer (which gets unmapped) is focused in which case the swee will
	 * receive focus. */
	detach(swee);
	for (pc = &swer->mon->clients; *pc && *pc != swer; pc = &(*pc)->next);
	*pc = swee;
	swee->next = swer->next;
	detachstack(swee);
	for (pc = &swer->mon->stack; *pc && *pc != swer; pc = &(*pc)->snext);
	*pc = swee;
	swee->snext = swer->snext;
	swee->mon = swer->mon;
	if (sweefocused) {
		detachstack(swee);
		attachstack(swee);
		selmon = swer->mon;
	}
	swee->tags = swer->tags;
	swee->isfloating = swer->isfloating;
	for (c = swee; c->swallowedby; c = c->swallowedby);
	c->swallowedby = swer;

	/* Configure geometry params obtained from patches (e.g. cfacts) here. */
	swee->cfact = swer->cfact;
	if(swer->canfocus == 0 && swee->canfocus == 1)
		swee->canfocus = 1;
	else if(swer->canfocus == 0 && swee->canfocus == 0) {
		swee->canfocus = 1;
		swflag = 1;
	} else
		swee->canfocus = swer->canfocus;

	/* ICCCM 4.1.3.1 */
	setclientstate(swer, WithdrawnState);
	if (manage)
		setclientstate(swee, NormalState);

	if (swee->isfloating || !swee->mon->lt[swee->mon->sellt]->arrange)
		XRaiseWindow(dpy, swee->win);
	resize(swee, swer->x, swer->y, swer->w, swer->h, 0);

	focus(NULL);
	arrange(NULL);
	if (manage)
		XMapWindow(dpy, swee->win);
	XUnmapWindow(dpy, swer->win);
	restack(swer->mon);
}

/*
 * Register a future swallow with swallower. 'c' 'class', 'inst' and 'title'
 * shall point null-terminated strings or be NULL, implying a wildcard. If an
 * already existing swallow instance targets 'c' its filters are updated and no
 * new swallow instance is created. 'c' may be ClientRegular or ClientSwallowee.
 * Complement to swalrm().
 */
void swalreg(Client *c, const char *class, const char *inst, const char *title)
{
	Swallow *s;

	if (!c)
		return;

	for (s = swallows; s; s = s->next) {
		if (s->client == c) {
			if (class)
				strncpy(s->class, class, sizeof(s->class) - 1);
			else
				s->class[0] = '\0';
			if (inst)
				strncpy(s->inst, inst, sizeof(s->inst) - 1);
			else
				s->inst[0] = '\0';
			if (title)
				strncpy(s->title, title, sizeof(s->title) - 1);
			else
				s->title[0] = '\0';
			s->decay = swaldecay;

			/* Only one swallow per client. May return after first hit. */
			return;
		}
	}

	s = ecalloc(1, sizeof(Swallow));
	s->decay = swaldecay;
	s->client = c;
	if (class)
		strncpy(s->class, class, sizeof(s->class) - 1);
	if (inst)
		strncpy(s->inst, inst, sizeof(s->inst) - 1);
	if (title)
		strncpy(s->title, title, sizeof(s->title) - 1);

	s->next = swallows;
	swallows = s;
}

/*
 * Decrease decay counter of all registered swallows by 'decayby' and remove any
 * swallow instances whose counter is less than or equal to zero.
 */
void
swaldecayby(int decayby)
{
	Swallow *s, *t;

	for (s = swallows; s; s = t) {
		s->decay -= decayby;
		t = s->next;
		if (s->decay <= 0)
			swalrm(s);
	}
}

/*
 * Window configuration and client setup for new windows which are to be
 * swallowed immediately. Pendant to manage() for such windows.
 */
void
swalmanage(Swallow *s, Window w, XWindowAttributes *wa)
{
	Client *swee, *swer;
	XWindowChanges wc;

	swer = s->client;
	swalrm(s);

	/* Perform bare minimum setup of a client for window 'w' such that swal()
	 * may be used to perform the swallow. The following lines are basically a
	 * minimal implementation of manage() with a few chunks delegated to
	 * swal(). */
	swee = ecalloc(1, sizeof(Client));
	swee->win = w;
	swee->mon = swer->mon;
	swee->oldbw = wa->border_width;
	swee->bw = borderpx;
	attach(swee);
	attachstack(swee);
	updatetitle(swee);
	updatesizehints(swee);
	XSelectInput(dpy, swee->win, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
	wc.border_width = swee->bw;
	XConfigureWindow(dpy, swee->win, CWBorderWidth, &wc);
	grabbuttons(swee, 0);
	XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
		(unsigned char *) &(swee->win), 1);

	swal(swer, swee, 1);
}

/*
 * Return swallow instance which targets window 'w' as determined by its class
 * name, instance name and window title. Returns NULL if none is found. Pendant
 * to wintoclient().
 */
Swallow *
swalmatch(Window w)
{
	XClassHint ch = { NULL, NULL };
	Swallow *s = NULL;
	char title[sizeof(s->title)];

	XGetClassHint(dpy, w, &ch);
	if (!gettextprop(w, netatom[NetWMName], title, sizeof(title)))
		gettextprop(w, XA_WM_NAME, title, sizeof(title));

	for (s = swallows; s; s = s->next) {
		if ((!ch.res_class || strstr(ch.res_class, s->class))
			&& (!ch.res_name || strstr(ch.res_name, s->inst))
			&& (title[0] == '\0' || strstr(title, s->title)))
			break;
	}

	if (ch.res_class)
		XFree(ch.res_class);
	if (ch.res_name)
		XFree(ch.res_name);
	return s;
}

/*
 * Interactive drag-and-drop swallow.
 */
void
swalmouse(const Arg *arg)
{
	Client *swer, *swee;
	XEvent ev;

	if (!(swee = selmon->sel))
		return;

	if (XGrabPointer(dpy, root, False, ButtonPressMask|ButtonReleaseMask, GrabModeAsync,
		GrabModeAsync, None, cursor[CurSwal]->cursor, CurrentTime) != GrabSuccess)
		return;

	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest: /* fallthrough */
		case Expose: /* fallthrough */
		case MapRequest:
			handler[ev.type](&ev);
			break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);

	if ((swer = wintoclient(ev.xbutton.subwindow))
		&& swer != swee)
		swal(swer, swee, 0);

	/* Remove accumulated pending EnterWindow events caused by the mouse
	 * movements. */
	XCheckMaskEvent(dpy, EnterWindowMask, &ev);
}

/*
 * Delete swallow instance swallows and free its resources. Complement to
 * swalreg(). If NULL is passed all swallows are deleted from.
 */
void
swalrm(Swallow *s)
{
	Swallow *t, **ps;

	if (s) {
		for (ps = &swallows; *ps && *ps != s; ps = &(*ps)->next);
		*ps = s->next;
		free(s);
	}
	else {
		for(s = swallows; s; s = t) {
			t = s->next;
			free(s);
		}
		swallows = NULL;
	}
}

/*
 * Removes swallow instance targeting 'c' if it exists. Complement to swalreg().
 */
void swalunreg(Client *c) { Swallow *s;

	for (s = swallows; s; s = s->next) {
		if (c == s->client) {
			swalrm(s);
			/* Max. 1 registered swallow per client. No need to continue. */
			break;
		}
	}
}

/*
 * Stop an active swallow of swallowed client 'swee' and remap the swallower.
 * If 'swee' is a swallower itself 'root' must point the root client of the
 * swallow chain containing 'swee'.
 */
void
swalstop(Client *swee, Client *root)
{
	Client *swer;

	if (!swee || !(swer = swee->swallowedby))
		return;

	swee->swallowedby = NULL;
	root = root ? root : swee;
	swer->mon = root->mon;
	swer->tags = root->tags;
	swer->next = root->next;
	root->next = swer;
	swer->snext = root->snext;
	root->snext = swer;
	swer->isfloating = swee->isfloating;

	/* Configure geometry params obtained from patches (e.g. cfacts) here. */
	swer->cfact = 1.0;
	if(swer->canfocus == 0 && swee->canfocus == 1) {
		swer->canfocus = 0;
		swee->canfocus = swflag ? 0 : 1;
		swflag = 0;
	} else if(swer->canfocus == 1 && swee->canfocus == 1) {
		swer->canfocus = 1;
		swee->canfocus = 1;
	} else if(swer->canfocus == 0 && swee->canfocus == 0) {
		swer->canfocus = 0;
		swee->canfocus = 0;
	}

	/* If swer is not in tiling mode reuse swee's geometry. */
	if (swer->isfloating || !root->mon->lt[root->mon->sellt]->arrange) {
		XRaiseWindow(dpy, swer->win);
		resize(swer, swee->x, swee->y, swee->w, swee->h, 0);
	}

	/* Override swer's border scheme which may be using SchemeSel. */
	XSetWindowBorder(dpy, swer->win, scheme[SchemeNorm][ColBorder].pixel);

	/* ICCCM 4.1.3.1 */
	setclientstate(swer, NormalState);

	XMapWindow(dpy, swer->win);
	focus(NULL);
	arrange(swer->mon);
}

/*
 * Stop active swallow for currently selected client.
 */
void
swalstopsel(const Arg *unused)
{
	if (selmon->sel)
		swalstop(selmon->sel, NULL);
}

void spawnscratch(const Arg *arg)
{
	if (fork() == 0) {
		if (dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[1], ((char **)arg->v)+1);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[1]);
		perror(" failed");
		exit(EXIT_SUCCESS);
	}
}

void
swapclient(const Arg *arg)
{
	Client *s, *m, t;

	if (!mark || !selmon->sel || mark == selmon->sel
	    || !selmon->lt[selmon->sellt]->arrange)
		return;
	s = selmon->sel;
	m = mark;
	t = *s;
	strcpy(s->name, m->name);
	s->win = m->win;
	s->x = m->x;
	s->y = m->y;
	s->w = m->w;
	s->h = m->h;

	m->win = t.win;
	strcpy(m->name, t.name);
	m->x = t.x;
	m->y = t.y;
	m->w = t.w;
	m->h = t.h;

	selmon->sel = m;
	mark = s;
	focus(s);
	setmark(m);

	arrange(s->mon);
	if (s->mon != m->mon) {
		arrange(m->mon);
	}
}

void
swapfocus(const Arg *arg)
{
	Client *t;

	if (!selmon->sel || !mark || selmon->sel == mark)
		return;
	t = selmon->sel;
	if (mark->mon != selmon) {
		unfocus(selmon->sel, 0);
		selmon = mark->mon;
	}
	if (ISVISIBLE(mark)) {
		focus(mark);
		restack(selmon);
	} else {
		selmon->seltags ^= 1;
		selmon->tagset[selmon->seltags] = mark->tags;
		focus(mark);
		arrange(selmon);
	}
	setmark(t);
}

void
togglemark(const Arg *arg)
{
	if (!selmon->sel)
		return;
	setmark(selmon->sel == mark ? 0 : selmon->sel);
}


void
tag(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		if (selmon->sel->switchtag)
			selmon->sel->switchtag = 0;
		focus(NULL);
		arrange(selmon);
	}
}

void
tagto(const Arg *arg)
{
	if (selmon->sel && arg->ui & TAGMASK) {
		selmon->sel->tags = arg->ui & TAGMASK;
		if (selmon->sel->switchtag)
			selmon->sel->switchtag = 0;
		focus(NULL);
		arrange(selmon);
		if(viewontag && ((arg->ui & TAGMASK) != TAGMASK))
			view(arg);
	}
}

void
tagmon(const Arg *arg)
{
	if (!selmon->sel || !mons->next)
		return;
	sendmon(selmon->sel, dirtomon(arg->i));
}

void
togglealttag()
{
	selmon->alttag = !selmon->alttag;
	drawbar(selmon);
}

void
togglebar(const Arg *arg)
{
	selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
	updatebarpos(selmon);
	resizebarwin(selmon);
	XMoveResizeWindow(dpy, selmon->extrabarwin, selmon->wx + sp, selmon->eby - vp, selmon->ww - 2 * sp, bh);
	if (showsystray) {
		XWindowChanges wc;
		if (!selmon->showbar)
			wc.y = -bh;
		else if (selmon->showbar) {
			wc.y = vp;
			if (!selmon->topbar)
				wc.y = selmon->mh - bh + vp;
		}
		XConfigureWindow(dpy, systray->win, CWY, &wc);
	}
	arrange(selmon);
}

void
tabmode(const Arg *arg)
{
	if(arg && arg->i >= 0)
		selmon->showtab = arg->ui % showtab_nmodes;
	else
		selmon->showtab = (selmon->showtab + 1 ) % showtab_nmodes;
	arrange(selmon);
}


void
togglefloating(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
	if (selmon->sel->isfloating)
		/* restore last known float dimensions */
		resize(selmon->sel, selmon->sel->sfx, selmon->sel->sfy,
		       selmon->sel->sfw, selmon->sel->sfh, False);
	else {
		/* save last known float dimensions */
		selmon->sel->sfx = selmon->sel->x;
		selmon->sel->sfy = selmon->sel->y;
		selmon->sel->sfw = selmon->sel->w;
		selmon->sel->sfh = selmon->sel->h;
		selmon->sel->isalwaysontop = 0; /* disabled, turn this off too */
	}
  	resetcanfocusfloating();
	arrange(selmon->sel->mon);
}

void
resetcanfocusfloating()
{
	unsigned int i, n;
	Client *c;

	for (n = 0, c = selmon->clients; c; c = c->next, n++);
	if (n == 0)
		return;

	for (i = 0, c = selmon->clients; c; c = c->next, i++)
    if (c->isfloating)
      c->cantfocus = 0;

	arrange(selmon);
}

void
togglecanfocusfloating(const Arg *arg)
{
	unsigned int n;
	Client *c, *cf = NULL;

  if (!selmon->sel)
      return;

  for (c = selmon->clients; c; c = c->next)
      if (c->cantfocus == 1) {
          cf = c;
      }

  if (cf) {
      resetcanfocusfloating();
      focus(cf);
  } else {
    for (n = 0, c = selmon->clients; c; c = c->next)
        if (c->isfloating)
            c->cantfocus = !c->cantfocus;
        else
            n++;

    if (n && selmon->sel->isfloating) {
        c = nexttiled(selmon->clients);
        focus(c);
    }
  }

	arrange(selmon);
}

void
togglefloatcenter(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
		return;
	if (selmon->sel->isfloating) {
		newx = selmon->sel->mon->mx + (selmon->sel->mon->mw - WIDTH(selmon->sel)) / 2;
		newy = selmon->sel->mon->my + (selmon->sel->mon->mh - HEIGHT(selmon->sel)) / 2;
		if (selmon->sel->x == newx && selmon->sel->y == newy){
			selmon->sel->x = tmpx;
			selmon->sel->y = tmpy;
		} else {
			tmpx = selmon->sel->x;
			tmpy = selmon->sel->y;
			selmon->sel->x = newx;
			selmon->sel->y = newy;
		}
	}
	arrange(selmon);
}

void
togglealwaysontop(const Arg *arg)
{
	if (!selmon->sel)
		return;
	if (selmon->sel->isfullscreen)
		return;

	if(selmon->sel->isalwaysontop){
		selmon->sel->isalwaysontop = 0;
	}else{
		/* disable others */
		for(Monitor *m = mons; m; m = m->next)
			for(Client *c = m->clients; c; c = c->next)
				c->isalwaysontop = 0;

		/* turn on, make it float too */
		selmon->sel->isfloating = 1;
		selmon->sel->isalwaysontop = 1;
	}

	arrange(selmon);
}

void
togglesticky(const Arg *arg)
{
	if (!selmon->sel)
		return;
	selmon->sel->issticky = !selmon->sel->issticky;
	arrange(selmon);
}

void
togglescratch(const Arg *arg)
{
	Client *c;
	unsigned int found = 0;

	for (c = selmon->clients; c && !(found = c->scratchkey == ((char**)arg->v)[0][0]); c = c->next);
	if (found) {
		c->tags = ISVISIBLE(c) ? 0 : selmon->tagset[selmon->seltags];
		focus(NULL);
		arrange(selmon);

		if (ISVISIBLE(c)) {
			focus(c);
			restack(selmon);
		}

	} else{
		spawnscratch(arg);
	}
}

void
togglefullscr(const Arg *arg)
{
  if(selmon->sel)
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;

	if (!selmon->sel)
		return;
	newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		selmon->sel->tags = newtags;
		focus(NULL);
		arrange(selmon);
	}
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
	int i;

	prevmon = NULL;
	switchtag();
	selmon->tagset[selmon->seltags] = newtagset;
	if (newtagset == ~0) {
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = 0;
	}

	/* test if the user did not select the same tag */
	if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
		selmon->pertag->prevtag = selmon->pertag->curtag;
		for (i = 0; !(newtagset & 1 << i); i++) ;
		selmon->pertag->curtag = i + 1;
	}

	/* apply settings for this view */
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);
	focus(NULL);
	arrange(selmon);
}

void
togglewin(const Arg *arg)
{
	Client *c = (Client*)arg->v;

	if (c == selmon->sel) {
		hidewin(c);
	} else {
		if (HIDDEN(c))
			showwin(c);
		focus(c);
		restack(selmon);
	}
}

void
freeicon(Client *c)
{
	if (c->icon) {
		XRenderFreePicture(dpy, c->icon);
		c->icon = None;
	}
}

void
unfocus(Client *c, int setfocus)
{
	if (!c)
		return;
	grabbuttons(c, 0);
	opacity(c, inactiveopacity);
	if (c == mark)
		XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColMark].pixel);
	else
		XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
	if (setfocus) {
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
		XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
	}
}

void
unmanage(Client *c, int destroyed)
{
	Monitor *m = c->mon;
	unsigned int switchtag = c->switchtag;
	XWindowChanges wc;

	/* Remove all swallow instances targeting client. */
	swalunreg(c);

	if (c == mark)
		setmark(0);

	detach(c);
	detachstack(c);
	freeicon(c);
	if (!destroyed) {
		wc.border_width = c->oldbw;
		XGrabServer(dpy); /* avoid race conditions */
		XSetErrorHandler(xerrordummy);
		XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		setclientstate(c, WithdrawnState);
		XSync(dpy, False);
		XSetErrorHandler(xerror);
		XUngrabServer(dpy);
	}
	if (scratchpad_last_showed == c)
		scratchpad_last_showed = NULL;
	free(c);
	focus(NULL);
	updateclientlist();
	arrange(m);
	if (switchtag)
		view(&((Arg) { .ui = switchtag }));
}

void
unmapnotify(XEvent *e)
{

	Client *c;
	XUnmapEvent *ev = &e->xunmap;
	int type;

	type = wintoclient2(ev->window, &c, NULL);
	if (type && ev->send_event) {
		setclientstate(c, WithdrawnState);
		return;
	}
	switch (type) {
	case ClientRegular:
		unmanage(c, 0);
		break;
	case ClientSwallowee:
		swalstop(c, NULL);
		unmanage(c, 0);
		break;
	case ClientSwallower:
		/* Swallowers are never mapped. Nothing to do. */
		break;
	}
	if ((c = wintosystrayicon(ev->window))) {
		/* KLUDGE! sometimes icons occasionally unmap their windows, but do
		 * _not_ destroy them. We map those windows back */
		XMapRaised(dpy, c->win);
		updatesystray();
	}
}

void
updatebars(void)
{
	unsigned int w;
	Monitor *m;
	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixel = 0,
		.border_pixel = 0,
		.colormap = cmap,
		.event_mask = ButtonPressMask|ExposureMask|PointerMotionMask
	};
	XClassHint ch = {"dwm", "dwm"};
	for (m = mons; m; m = m->next) {
		w = m->ww;
		if (showsystray && m == systraytomon(m))
			w -= getsystraywidth();
		if (!m->barwin) {
			m->barwin = XCreateWindow(dpy, root, m->wx + sp, m->by + vp, w - 2 * sp, bh, 0, depth,
						  InputOutput, visual,
						  CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
			XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
			if (showsystray && m == systraytomon(m))
				XMapRaised(dpy, systray->win);
			XMapRaised(dpy, m->barwin);
			XSetClassHint(dpy, m->barwin, &ch);
		}
		if (!m->extrabarwin) {
			m->extrabarwin = XCreateWindow(dpy, root, m->wx + sp, m->eby - vp, m->ww - 2 * sp, bh, 0, depth,
						  InputOutput, visual,
						  CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
			XDefineCursor(dpy, m->extrabarwin, cursor[CurNormal]->cursor);
			XMapRaised(dpy, m->extrabarwin);
			XSetClassHint(dpy, m->extrabarwin, &ch);
		}
		if (!m->tabwin) {
			m->tabwin = XCreateWindow(dpy, root, m->wx, m->ty, m->ww, th, 0, depth,
						  InputOutput, visual,
						  CWOverrideRedirect|CWBackPixel|CWBorderPixel|CWColormap|CWEventMask, &wa);
			XDefineCursor(dpy, m->tabwin, cursor[CurNormal]->cursor);
			XMapRaised(dpy, m->tabwin);
			XSetClassHint(dpy, m->tabwin, &ch);
		}
	}
}

void
updatebarpos(Monitor *m)
{
	Client *c;
	int nvis = 0;
	m->wy = m->my;
	m->wh = m->mh;
	m->wh -= bh * m->showbar * 2;
	m->wy = m->showbar ? m->wy + bh : m->wy;
	if (m->showbar) {
		m->by = m->topbar ? m->wy - bh : m->wy + m->wh;
		m->eby = m->topbar ? m->wy + m->wh : m->wy - bh;
	} else {
		m->by = -bh - vp;
		m->eby = -bh + vp;
	}
	for(c = m->clients; c; c = c->next) {
		if(ISVISIBLE(c)) ++nvis;
	}

	if(m->showtab == showtab_always
	   || ((m->showtab == showtab_auto) && (nvis > 1) && (m->lt[m->sellt]->arrange == monocle))) {
			if (!topbar)
				vp = -vp;
			m->ty = toptab ? m->wy +  gappoh + vp  : m->wy + m->wh - vp - gappoh - th;
			if (!topbar)
				vp = -vp;
	} else {
		m->ty = -th;
	}
	if(!(m->lt[m->sellt]->arrange == monocle)) {
		m->wx += 0;
		m->wy += gappoh;
		m->ww -= 0 + 0;
		m->wh -= 0 + gappoh * 2;
	}
}

void
updateclientlist()
{
	Client *c, *d;
	Monitor *m;

	XDeleteProperty(dpy, root, netatom[NetClientList]);
	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			for (d = c; d; d = d->swallowedby) {
				XChangeProperty(dpy, root, netatom[NetClientList],
					XA_WINDOW, 32, PropModeAppend,
					(unsigned char *) &(c->win), 1);
			}
		}
	}
}

int
updategeom(void)
{
	int dirty = 0;

#ifdef XINERAMA
	if (XineramaIsActive(dpy)) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
		XineramaScreenInfo *unique = NULL;

		for (n = 0, m = mons; m; m = m->next, n++);
		/* only consider unique geometries as separate screens */
		unique = ecalloc(nn, sizeof(XineramaScreenInfo));
		for (i = 0, j = 0; i < nn; i++)
			if (isuniquegeom(unique, j, &info[i]))
				memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
		XFree(info);
		nn = j;
		if (n <= nn) { /* new monitors available */
			for (i = 0; i < (nn - n); i++) {
				for (m = mons; m && m->next; m = m->next);
				if (m)
					m->next = createmon();
				else
					mons = createmon();
			}
			for (i = 0, m = mons; i < nn && m; m = m->next, i++)
				if (i >= n
				|| unique[i].x_org != m->mx || unique[i].y_org != m->my
				|| unique[i].width != m->mw || unique[i].height != m->mh)
				{
					dirty = 1;
					m->num = i;
					m->mx = m->wx = unique[i].x_org;
					m->my = m->wy = unique[i].y_org;
					m->mw = m->ww = unique[i].width;
					m->mh = m->wh = unique[i].height;
					updatebarpos(m);
				}
		} else { /* less monitors available nn < n */
			for (i = nn; i < n; i++) {
				for (m = mons; m && m->next; m = m->next);
				while ((c = m->clients)) {
					dirty = 1;
					m->clients = c->next;
					detachstack(c);
					c->mon = mons;
					attach(c);
					attachstack(c);
				}
				if (m == selmon)
					selmon = mons;
				cleanupmon(m);
			}
		}
		free(unique);
	} else
#endif /* XINERAMA */
	{ /* default monitor setup */
		if (!mons)
			mons = createmon();
		if (mons->mw != sw || mons->mh != sh) {
			dirty = 1;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos(mons);
		}
	}
	if (dirty) {
		selmon = mons;
		selmon = wintomon(root);
	}
	return dirty;
}

void
updatemotifhints(Client *c)
{
	Atom real;
	int format;
	unsigned char *p = NULL;
	unsigned long n, extra;
	unsigned long *motif;
	int width, height;

	if (!decorhints)
		return;

	if (XGetWindowProperty(dpy, c->win, motifatom, 0L, 5L, False, motifatom,
	                       &real, &format, &n, &extra, &p) == Success && p != NULL) {
		motif = (unsigned long*)p;
		if (motif[MWM_HINTS_FLAGS_FIELD] & MWM_HINTS_DECORATIONS) {
			width = WIDTH(c);
			height = HEIGHT(c);

			if (motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_ALL ||
			    motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_BORDER ||
			    motif[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_TITLE)
				c->bw = c->oldbw = borderpx;
			else
				c->bw = c->oldbw = 0;

			resize(c, c->x, c->y, width - (2*c->bw), height - (2*c->bw), 0);
		}
		XFree(p);
	}
}

void
updatenumlockmask(void)
{
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j]
				== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatestatus(void)
{
	char text[512];
	if (!gettextprop(root, XA_WM_NAME, text, sizeof(text))) {
		strcpy(stext, "dwm-"VERSION);
		estext[0] = '\0';
		strcpy(estext, "dwm-"VERSION);
	} else {
		char *e = strchr(text, statussep);
		if (e) {
			*e = '\0'; e++;
			strncpy(estext, e, sizeof(estext) - 1);
		} else {
			estext[0] = '\0';
		}
		strncpy(stext, text, sizeof(stext) - 1);
	}
	drawbar(selmon);
	updatesystray();
}

void
updatesystrayicongeom(Client *i, int w, int h)
{
	int rh = bh - vertpadbar;
	if (i) {
		i->h = rh;
		if (w == h)
			i->w = rh;
		else if (h == rh)
			i->w = w;
		else
			i->w = (int) ((float)rh * ((float)w / (float)h));
		i->y += vertpadbar / 2;
		applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
		/* force icons into the systray dimensions if they don't want to */
		if (i->h > rh) {
			if (i->w == i->h)
				i->w = rh;
			else
				i->w = (int) ((float)rh * ((float)i->w / (float)i->h));
			i->h = rh;
		}
	}
}

void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
	long flags;
	int code = 0;

	if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
			!(flags = getatomprop(i, xatom[XembedInfo])))
		return;

	if (flags & XEMBED_MAPPED && !i->tags) {
		i->tags = 1;
		code = XEMBED_WINDOW_ACTIVATE;
		XMapRaised(dpy, i->win);
		setclientstate(i, NormalState);
	}
	else if (!(flags & XEMBED_MAPPED) && i->tags) {
		i->tags = 0;
		code = XEMBED_WINDOW_DEACTIVATE;
		XUnmapWindow(dpy, i->win);
		setclientstate(i, WithdrawnState);
	}
	else
		return;
	sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
			systray->win, XEMBED_EMBEDDED_VERSION);
}

void
updatesystray(void)
{
	XSetWindowAttributes wa;
	XWindowChanges wc;
	Client *i;
	Monitor *m = systraytomon(NULL);
	unsigned int x = m->mx + m->mw;
	unsigned int sw = TEXTW(stext) - lrpad + systrayspacing;
	unsigned int w = 1;

	if (!showsystray)
		return;
	if (systrayonleft)
		x -= sw + lrpad / 2;
	if (!systray) {
		/* init systray */
		if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
			die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
		systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
		wa.event_mask        = ButtonPressMask | ExposureMask;
		wa.override_redirect = True;
		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XSelectInput(dpy, systray->win, SubstructureNotifyMask);
		XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
				PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
		XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
		XMapRaised(dpy, systray->win);
		XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
		if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
			sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
			XSync(dpy, False);
		}
		else {
			fprintf(stderr, "dwm: unable to obtain system tray.\n");
			free(systray);
			systray = NULL;
			return;
		}
	}
	for (w = 0, i = systray->icons; i; i = i->next) {
		/* make sure the background color stays the same */
		wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
		XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
		XMapRaised(dpy, i->win);
		w += systrayspacing;
		i->x = w;
		XMoveResizeWindow(dpy, i->win, i->x, vertpadbar / 2, i->w, i->h);
		w += i->w;
		if (i->mon != m)
			i->mon = m;
	}
	w = w ? w + systrayspacing : 1;
	x -= w;
	XMoveResizeWindow(dpy, systray->win, x - sp, m->by + vp, w, bh);
	wc.x = x - sp; wc.y = m->by + vp; wc.width = w; wc.height = bh;
	wc.stack_mode = Above; wc.sibling = m->barwin;
	XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
	XMapWindow(dpy, systray->win);
	XMapSubwindows(dpy, systray->win);
	/* redraw background */
	XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
	XFillRectangle(dpy, systray->win, XCreateGC(dpy, root, 0, NULL), w, -bh, w, bh);
	XSync(dpy, False);
}

void
updatetitle(Client *c)
{
	char oldname[sizeof(c->name)];
	strcpy(oldname, c->name);

	if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if (c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);

	for (Monitor *m = mons; m; m = m->next) {
		if (m->sel == c && strcmp(oldname, c->name) != 0)
			ipc_focused_title_change_event(m->num, c->win, oldname, c->name);
	}
}

void
updatepreview(void)
{
	Monitor *m;

	XSetWindowAttributes wa = {
		.override_redirect = True,
		.background_pixmap = ParentRelative,
		.event_mask = ButtonPressMask|ExposureMask
	};
	for (m = mons; m; m = m->next) {
		m->tagwin = XCreateWindow(dpy, root, m->wx + sp, (topbar ? m->by + bh + vp : m->by + vp - m->mh / scalepreview), m->mw / scalepreview, m->mh / scalepreview, 0,
				DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
				CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->tagwin, cursor[CurNormal]->cursor);
		XMapRaised(dpy, m->tagwin);
		XUnmapWindow(dpy, m->tagwin);
	}
}

void
updateicon(Client *c)
{
	freeicon(c);
	c->icon = geticonprop(c->win, &c->icw, &c->ich);
}

void
updatewindowtype(Client *c)
{
	Atom state = getatomprop(c, netatom[NetWMState]);
	Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

	if (state == netatom[NetWMFullscreen])
		setfullscreen(c, 1);
	if (wtype == netatom[NetWMWindowTypeDialog]) {
		c->iscentered = 1;
		c->isfloating = 1;
	}
}

void
updatewmhints(Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win))) {
		if (c == selmon->sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
		if (wmh->flags & InputHint)
			c->neverfocus = !wmh->input;
		else
			c->neverfocus = 0;
		XFree(wmh);
	}
}

void
view(const Arg *arg)
{
	int i;
	unsigned int tmptag;

	if(arg->ui && (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	prevmon = NULL;
	switchtag();
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
		togglebar(NULL);

	focus(NULL);
	arrange(selmon);
}

pid_t
winpid(Window w)
{
	pid_t result = 0;

#ifdef __linux__
	xcb_res_client_id_spec_t spec = {0};
	spec.client = w;
	spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

	xcb_generic_error_t *e = NULL;
	xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
	xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

	if (!r)
		return (pid_t)0;

	xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
	for (; i.rem; xcb_res_client_id_value_next(&i)) {
		spec = i.data->spec;
		if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
			uint32_t *t = xcb_res_client_id_value_value(i.data);
			result = *t;
			break;
		}
	}

	free(r);

	if (result == (pid_t)-1)
		result = 0;

#endif /* __linux__ */

#ifdef __OpenBSD__
		Atom type;
		int format;
		unsigned long len, bytes;
		unsigned char *prop;
		pid_t ret;

		if (XGetWindowProperty(dpy, w, XInternAtom(dpy, "_NET_WM_PID", 0), 0, 1, False, AnyPropertyType, &type, &format, &len, &bytes, &prop) != Success || !prop)
			return 0;

		ret = *(pid_t*)prop;
		XFree(prop);
		result = ret;

#endif /* __OpenBSD__ */
	return result;
}

Client *
wintoclient(Window w)
{
	Client *c;
	Monitor *m;

	for (m = mons; m; m = m->next)
		for (c = m->clients; c; c = c->next)
			if (c->win == w)
				return c;
	return NULL;
}

Client *
wintosystrayicon(Window w) {
	Client *i = NULL;

	if (!showsystray || !w)
		return i;
	for (i = systray->icons; i && i->win != w; i = i->next) ;
	return i;
}

/*
 * Writes client managing window 'w' into 'pc' and returns type of client. If
 * no client is found NULL is written to 'pc' and zero is returned. If a client
 * is found and is a swallower (ClientSwallower) and proot is not NULL the root
 * client of the swallow chain is written to 'proot'.
 */
int
wintoclient2(Window w, Client **pc, Client **proot)
{
	Monitor *m;
	Client *c, *d;

	for (m = mons; m; m = m->next) {
		for (c = m->clients; c; c = c->next) {
			if (c->win == w) {
				*pc = c;
				if (c->swallowedby)
					return ClientSwallowee;
				else
					return ClientRegular;
			}
			else {
				for (d = c->swallowedby; d; d = d->swallowedby) {
					if (d->win == w) {
						if (proot)
							*proot = c;
						*pc = d;
						return ClientSwallower;
					}
				}
			}
		}
	}
	*pc = NULL;
	return 0;
}

Monitor *
wintomon(Window w)
{
	int x, y;
	Client *c;
	Monitor *m;

	if (w == root && getrootptr(&x, &y))
		return recttomon(x, y, 1, 1);
	for (m = mons; m; m = m->next)
		if (w == m->barwin || w == m->extrabarwin || w == m->tabwin)
			return m;
	if ((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* Selects for the view of the focused window. The list of tags */
/* to be displayed is matched to the focused window tag list. */
void
winview(const Arg* arg){
	Window win, win_r, win_p, *win_c;
	unsigned nc;
	int unused;
	Client* c;
	Arg a;

	if (!XGetInputFocus(dpy, &win, &unused)) return;
	while(XQueryTree(dpy, win, &win_r, &win_p, &win_c, &nc)
	      && win_p != win_r) win = win_p;

	if (!(c = wintoclient(win))) return;

	a.ui = c->tags;
	view(&a);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
		ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	die("dwm: another window manager is already running");
	return -1;
}

Monitor *
systraytomon(Monitor *m) {
	Monitor *t;
	int i, n;
	if(!systraypinning) {
		if(!m)
			return selmon;
		return m == selmon ? m : NULL;
	}
	for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
	for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
	if(systraypinningfailfirst && n < systraypinning)
		return mons;
	return t;
}

void
xinitvisual()
{
	XVisualInfo *infos;
	XRenderPictFormat *fmt;
	int nitems;
	int i;

	XVisualInfo tpl = {
		.screen = screen,
		.depth = 32,
		.class = TrueColor
	};
	long masks = VisualScreenMask | VisualDepthMask | VisualClassMask;

	infos = XGetVisualInfo(dpy, masks, &tpl, &nitems);
	visual = NULL;
	for(i = 0; i < nitems; i ++) {
		fmt = XRenderFindVisualFormat(dpy, infos[i].visual);
		if (fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
			visual = infos[i].visual;
			depth = infos[i].depth;
			cmap = XCreateColormap(dpy, root, visual, AllocNone);
			useargb = 1;
			break;
		}
	}

	XFree(infos);

	if (! visual) {
		visual = DefaultVisual(dpy, screen);
		depth = DefaultDepth(dpy, screen);
		cmap = DefaultColormap(dpy, screen);
	}
}

void
zoom(const Arg *arg)
{
	Client *c = selmon->sel;
	Client *at = NULL, *cold, *cprevious = NULL;

	if (!selmon->lt[selmon->sellt]->arrange
	|| (selmon->sel && selmon->sel->isfloating))
		return;
	if (c == nexttiled(selmon->clients)) {
		at = findbefore(prevzoom);
		if (at)
			cprevious = nexttiled(at->next);
		if (!cprevious || cprevious != prevzoom) {
			prevzoom = NULL;
			if (!c || !(c = nexttiled(c->next)))
				return;
		} else
			c = cprevious;
	}
	cold = nexttiled(selmon->clients);
	if (c != cold && !at)
		at = findbefore(c);
	detach(c);
	attach(c);
	/* swap windows instead of pushing the previous one down */
	if (c != cold && at) {
		prevzoom = cold;
		if (cold && at != cold) {
			detach(cold);
			cold->next = at->next;
			at->next = cold;
		}
	}
	focus(c);
	arrange(c->mon);
}

void
setgaps(int oh, int ov, int ih, int iv)
{
	if (oh < 0) oh = 0;
	if (ov < 0) ov = 0;
	if (ih < 0) ih = 0;
	if (iv < 0) iv = 0;

	selmon->gappoh = oh;
	selmon->gappov = ov;
	selmon->gappih = ih;
	selmon->gappiv = iv;
	arrange(selmon);
}

void
togglegaps(const Arg *arg)
{
	#if PERTAG_PATCH
	selmon->pertag->enablegaps[selmon->pertag->curtag] = !selmon->pertag->enablegaps[selmon->pertag->curtag];
	#else
	enablegaps = !enablegaps;
	#endif // PERTAG_PATCH
	arrange(NULL);
}

void
defaultgaps(const Arg *arg)
{
	setgaps(gappoh, gappov, gappih, gappiv);
}

void
incrgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov + arg->i,
		selmon->gappih + arg->i,
		selmon->gappiv + arg->i
	);
}

void
incrigaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih + arg->i,
		selmon->gappiv + arg->i
	);
}

void
incrogaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov + arg->i,
		selmon->gappih,
		selmon->gappiv
	);
}

void
incrohgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov,
		selmon->gappih,
		selmon->gappiv
	);
}

void
incrovgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov + arg->i,
		selmon->gappih,
		selmon->gappiv
	);
}

void
incrihgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih + arg->i,
		selmon->gappiv
	);
}

void
incrivgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih,
		selmon->gappiv + arg->i
	);
}

void
getgaps(Monitor *m, int *oh, int *ov, int *ih, int *iv, unsigned int *nc)
{
	unsigned int n, oe, ie;
	#if PERTAG_PATCH
	oe = ie = selmon->pertag->enablegaps[selmon->pertag->curtag];
	#else
	oe = ie = enablegaps;
	#endif // PERTAG_PATCH
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
	if (smartgaps && n == 1) {
		oe = 0; // outer gaps disabled when only one client
	}

	*oh = m->gappoh*oe; // outer horizontal gap
	*ov = m->gappov*oe; // outer vertical gap
	*ih = m->gappih*ie; // inner horizontal gap
	*iv = m->gappiv*ie; // inner vertical gap
	*nc = n;            // number of clients
}

void
getfacts(Monitor *m, int msize, int ssize, float *mf, float *sf, int *mr, int *sr)
{
	unsigned int n;
	float mfacts = 0, sfacts = 0;
	int mtotal = 0, stotal = 0;
	Client *c;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		if (n < m->nmaster)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		if (n < m->nmaster)
			mtotal += msize * (c->cfact / mfacts);
		else
			stotal += ssize * (c->cfact / sfacts);

	*mf = mfacts; // total factor of master area
	*sf = sfacts; // total factor of stack area
	*mr = msize - mtotal; // the remainder (rest) of pixels after a cfacts master split
	*sr = ssize - stotal; // the remainder (rest) of pixels after a cfacts stack split
}

/***
 * Layouts
 */

/*
 * Bottomstack layout + gaps
 * https://dwm.suckless.org/patches/bottomstack/
 */
static void
bstack(Monitor *m)
{
	unsigned int i, n;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	float mfacts, sfacts;
	int mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh;
	mw = m->ww - 2*ov - iv * (MIN(n, m->nmaster) - 1);
	sw = m->ww - 2*ov - iv * (n - m->nmaster - 1);

	if (m->nmaster && n > m->nmaster) {
		sh = (mh - ih) * (1 - m->mfact);
		mh = mh - ih - sh;
		sx = mx;
		sy = my + mh + ih;
	}

	getfacts(m, mw, sw, &mfacts, &sfacts, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (i < m->nmaster) {
			resize(c, mx, my, mw * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv;
		} else {
			resize(c, sx, sy, sw * (c->cfact / sfacts) + ((i - m->nmaster) < srest ? 1 : 0) - (2*c->bw), sh - (2*c->bw), 0);
			sx += WIDTH(c) + iv;
		}
	}
}

static void
bstackhoriz(Monitor *m)
{
	unsigned int i, n;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	float mfacts, sfacts;
	int mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	mh = m->wh - 2*oh;
	sh = m->wh - 2*oh - ih * (n - m->nmaster - 1);
	mw = m->ww - 2*ov - iv * (MIN(n, m->nmaster) - 1);
	sw = m->ww - 2*ov;

	if (m->nmaster && n > m->nmaster) {
		sh = (mh - ih) * (1 - m->mfact);
		mh = mh - ih - sh;
		sy = my + mh + ih;
		sh = m->wh - mh - 2*oh - ih * (n - m->nmaster);
	}

	getfacts(m, mw, sh, &mfacts, &sfacts, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (i < m->nmaster) {
			resize(c, mx, my, mw * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv;
		} else {
			resize(c, sx, sy, sw - (2*c->bw), sh * (c->cfact / sfacts) + ((i - m->nmaster) < srest ? 1 : 0) - (2*c->bw), 0);
			sy += HEIGHT(c) + ih;
		}
	}
}

/*
 * Centred master layout + gaps
 * https://dwm.suckless.org/patches/centeredmaster/
 */
void
centeredmaster(Monitor *m)
{
	unsigned int i, n;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int lx = 0, ly = 0, lw = 0, lh = 0;
	int rx = 0, ry = 0, rw = 0, rh = 0;
	float mfacts = 0, lfacts = 0, rfacts = 0;
	int mtotal = 0, ltotal = 0, rtotal = 0;
	int mrest = 0, lrest = 0, rrest = 0;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	/* initialize areas */
	mx = m->wx + ov;
	my = m->wy + oh;
	mh = m->wh - 2*oh - ih * ((!m->nmaster ? n : MIN(n, m->nmaster)) - 1);
	mw = m->ww - 2*ov;
	lh = m->wh - 2*oh - ih * (((n - m->nmaster) / 2) - 1);
	rh = m->wh - 2*oh - ih * (((n - m->nmaster) / 2) - ((n - m->nmaster) % 2 ? 0 : 1));

	if (m->nmaster && n > m->nmaster) {
		/* go mfact box in the center if more than nmaster clients */
		if (n - m->nmaster > 1) {
			/* ||<-S->|<---M--->|<-S->|| */
			mw = (m->ww - 2*ov - 2*iv) * m->mfact;
			lw = (m->ww - mw - 2*ov - 2*iv) / 2;
			rw = (m->ww - mw - 2*ov - 2*iv) - lw;
			mx += lw + iv;
		} else {
			/* ||<---M--->|<-S->|| */
			mw = (mw - iv) * m->mfact;
			lw = 0;
			rw = m->ww - mw - iv - 2*ov;
		}
		lx = m->wx + ov;
		ly = m->wy + oh;
		rx = mx + mw + iv;
		ry = m->wy + oh;
	}

	/* calculate facts */
	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
		if (!m->nmaster || n < m->nmaster)
			mfacts += c->cfact;
		else if ((n - m->nmaster) % 2)
			lfacts += c->cfact; // total factor of left hand stack area
		else
			rfacts += c->cfact; // total factor of right hand stack area
	}

	for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
		if (!m->nmaster || n < m->nmaster)
			mtotal += mh * (c->cfact / mfacts);
		else if ((n - m->nmaster) % 2)
			ltotal += lh * (c->cfact / lfacts);
		else
			rtotal += rh * (c->cfact / rfacts);

	mrest = mh - mtotal;
	lrest = lh - ltotal;
	rrest = rh - rtotal;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		if (!m->nmaster || i < m->nmaster) {
			/* nmaster clients are stacked vertically, in the center of the screen */
			resize(c, mx, my, mw - (2*c->bw), mh * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			/* stack clients are stacked vertically */
			if ((i - m->nmaster) % 2 ) {
				resize(c, lx, ly, lw - (2*c->bw), lh * (c->cfact / lfacts) + ((i - 2*m->nmaster) < 2*lrest ? 1 : 0) - (2*c->bw), 0);
				ly += HEIGHT(c) + ih;
			} else {
				resize(c, rx, ry, rw - (2*c->bw), rh * (c->cfact / rfacts) + ((i - 2*m->nmaster) < 2*rrest ? 1 : 0) - (2*c->bw), 0);
				ry += HEIGHT(c) + ih;
			}
		}
	}
}

void
centeredfloatingmaster(Monitor *m)
{
	unsigned int i, n;
	float mfacts, sfacts;
	float mivf = 1.0; // master inner vertical gap factor
	int oh, ov, ih, iv, mrest, srest;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh;
	mw = m->ww - 2*ov - iv*(n - 1);
	sw = m->ww - 2*ov - iv*(n - m->nmaster - 1);

	if (m->nmaster && n > m->nmaster) {
		mivf = 0.8;
		/* go mfact box in the center if more than nmaster clients */
		if (m->ww > m->wh) {
			mw = m->ww * m->mfact - iv*mivf*(MIN(n, m->nmaster) - 1);
			mh = m->wh * 0.9;
		} else {
			mw = m->ww * 0.9 - iv*mivf*(MIN(n, m->nmaster) - 1);
			mh = m->wh * m->mfact;
		}
		mx = m->wx + (m->ww - mw) / 2;
		my = m->wy + (m->wh - mh - 2*oh) / 2;

		sx = m->wx + ov;
		sy = m->wy + oh;
		sh = m->wh - 2*oh;
	}

	getfacts(m, mw, sw, &mfacts, &sfacts, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			/* nmaster clients are stacked horizontally, in the center of the screen */
			resize(c, mx, my, mw * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv*mivf;
		} else {
			/* stack clients are stacked horizontally */
			resize(c, sx, sy, sw * (c->cfact / sfacts) + ((i - m->nmaster) < srest ? 1 : 0) - (2*c->bw), sh - (2*c->bw), 0);
			sx += WIDTH(c) + iv;
		}
}

/*
 * Deck layout + gaps
 * https://dwm.suckless.org/patches/deck/
 */
void
deck(Monitor *m)
{
	unsigned int i, n;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	float mfacts, sfacts;
	int mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh - ih * (MIN(n, m->nmaster) - 1);
	sw = mw = m->ww - 2*ov;

	if (m->nmaster && n > m->nmaster) {
		sw = (mw - iv) * (1 - m->mfact);
		mw = mw - iv - sw;
		sx = mx + mw + iv;
		sh = m->wh - 2*oh;
	}

	getfacts(m, mh, sh, &mfacts, &sfacts, &mrest, &srest);

	if (n - m->nmaster > 0) /* override layout symbol */
		snprintf(m->ltsymbol, sizeof m->ltsymbol, "D %d", n - m->nmaster);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			resize(c, mx, my, mw - (2*c->bw), mh * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			resize(c, sx, sy, sw - (2*c->bw), sh - (2*c->bw), 0);
		}
}

/*
 * Fibonacci layout + gaps
 * https://dwm.suckless.org/patches/fibonacci/
 */
void
fibonacci(Monitor *m, int s)
{
	unsigned int i, n;
	int nx, ny, nw, nh;
	int oh, ov, ih, iv;
	int nv, hrest = 0, wrest = 0, r = 1;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	nx = m->wx + ov;
	ny = m->wy + oh;
	nw = m->ww - 2*ov;
	nh = m->wh - 2*oh;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
		if (r) {
			if ((i % 2 && (nh - ih) / 2 <= (bh + 2*c->bw))
			   || (!(i % 2) && (nw - iv) / 2 <= (bh + 2*c->bw))) {
				r = 0;
			}
			if (r && i < n - 1) {
				if (i % 2) {
					nv = (nh - ih) / 2;
					hrest = nh - 2*nv - ih;
					nh = nv;
				} else {
					nv = (nw - iv) / 2;
					wrest = nw - 2*nv - iv;
					nw = nv;
				}

				if ((i % 4) == 2 && !s)
					nx += nw + iv;
				else if ((i % 4) == 3 && !s)
					ny += nh + ih;
			}

			if ((i % 4) == 0) {
				if (s) {
					ny += nh + ih;
					nh += hrest;
				}
				else {
					nh -= hrest;
					ny -= nh + ih;
				}
			}
			else if ((i % 4) == 1) {
				nx += nw + iv;
				nw += wrest;
			}
			else if ((i % 4) == 2) {
				ny += nh + ih;
				nh += hrest;
				if (i < n - 1)
					nw += wrest;
			}
			else if ((i % 4) == 3) {
				if (s) {
					nx += nw + iv;
					nw -= wrest;
				} else {
					nw -= wrest;
					nx -= nw + iv;
					nh += hrest;
				}
			}
			if (i == 0)	{
				if (n != 1) {
					nw = (m->ww - iv - 2*ov) - (m->ww - iv - 2*ov) * (1 - m->mfact);
					wrest = 0;
				}
				ny = m->wy + oh;
			}
			else if (i == 1)
				nw = m->ww - nw - iv - 2*ov;
			i++;
		}

		resize(c, nx, ny, nw - (2*c->bw), nh - (2*c->bw), False);
	}
}

void
dwindle(Monitor *m)
{
	fibonacci(m, 1);
}

void
spiral(Monitor *m)
{
	fibonacci(m, 0);
}

/*
 * Gappless grid layout + gaps (ironically)
 * https://dwm.suckless.org/patches/gaplessgrid/
 */
void
gaplessgrid(Monitor *m)
{
	unsigned int i, n;
	int x, y, cols, rows, ch, cw, cn, rn, rrest, crest; // counters
	int oh, ov, ih, iv;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	/* grid dimensions */
	for (cols = 0; cols <= n/2; cols++)
		if (cols*cols >= n)
			break;
	if (n == 5) /* set layout against the general calculation: not 1:2:2, but 2:3 */
		cols = 2;
	rows = n/cols;
	cn = rn = 0; // reset column no, row no, client count

	ch = (m->wh - 2*oh - ih * (rows - 1)) / rows;
	cw = (m->ww - 2*ov - iv * (cols - 1)) / cols;
	rrest = (m->wh - 2*oh - ih * (rows - 1)) - ch * rows;
	crest = (m->ww - 2*ov - iv * (cols - 1)) - cw * cols;
	x = m->wx + ov;
	y = m->wy + oh;

	for (i = 0, c = nexttiled(m->clients); c; i++, c = nexttiled(c->next)) {
		if (i/rows + 1 > cols - n%cols) {
			rows = n/cols + 1;
			ch = (m->wh - 2*oh - ih * (rows - 1)) / rows;
			rrest = (m->wh - 2*oh - ih * (rows - 1)) - ch * rows;
		}
		resize(c,
			x,
			y + rn*(ch + ih) + MIN(rn, rrest),
			cw + (cn < crest ? 1 : 0) - 2*c->bw,
			ch + (rn < rrest ? 1 : 0) - 2*c->bw,
			0);
		rn++;
		if (rn >= rows) {
			rn = 0;
			x += cw + ih + (cn < crest ? 1 : 0);
			cn++;
		}
	}
}

/*
 * Gridmode layout + gaps
 * https://dwm.suckless.org/patches/gridmode/
 */
void
grid(Monitor *m)
{
	unsigned int i, n;
	int cx, cy, cw, ch, cc, cr, chrest, cwrest, cols, rows;
	int oh, ov, ih, iv;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);

	/* grid dimensions */
	for (rows = 0; rows <= n/2; rows++)
		if (rows*rows >= n)
			break;
	cols = (rows && (rows - 1) * rows >= n) ? rows - 1 : rows;

	/* window geoms (cell height/width) */
	ch = (m->wh - 2*oh - ih * (rows - 1)) / (rows ? rows : 1);
	cw = (m->ww - 2*ov - iv * (cols - 1)) / (cols ? cols : 1);
	chrest = (m->wh - 2*oh - ih * (rows - 1)) - ch * rows;
	cwrest = (m->ww - 2*ov - iv * (cols - 1)) - cw * cols;
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
		cc = i / rows;
		cr = i % rows;
		cx = m->wx + ov + cc * (cw + iv) + MIN(cc, cwrest);
		cy = m->wy + oh + cr * (ch + ih) + MIN(cr, chrest);
		resize(c, cx, cy, cw + (cc < cwrest ? 1 : 0) - 2*c->bw, ch + (cr < chrest ? 1 : 0) - 2*c->bw, False);
	}
}

/*
 * Horizontal grid layout + gaps
 * https://dwm.suckless.org/patches/horizgrid/
 */
void
horizgrid(Monitor *m) {
	Client *c;
	unsigned int n, i;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	int ntop, nbottom = 1;
	float mfacts = 0, sfacts = 0;
	int mrest, srest, mtotal = 0, stotal = 0;

	/* Count windows */
	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	if (n <= 2)
		ntop = n;
	else {
		ntop = n / 2;
		nbottom = n - ntop;
	}
	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	sh = mh = m->wh - 2*oh;
	sw = mw = m->ww - 2*ov;

	if (n > ntop) {
		sh = (mh - ih) / 2;
		mh = mh - ih - sh;
		sy = my + mh + ih;
		mw = m->ww - 2*ov - iv * (ntop - 1);
		sw = m->ww - 2*ov - iv * (nbottom - 1);
	}

	/* calculate facts */
	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < ntop)
			mfacts += c->cfact;
		else
			sfacts += c->cfact;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < ntop)
			mtotal += mh * (c->cfact / mfacts);
		else
			stotal += sw * (c->cfact / sfacts);

	mrest = mh - mtotal;
	srest = sw - stotal;

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < ntop) {
			resize(c, mx, my, mw * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), mh - (2*c->bw), 0);
			mx += WIDTH(c) + iv;
		} else {
			resize(c, sx, sy, sw * (c->cfact / sfacts) + ((i - ntop) < srest ? 1 : 0) - (2*c->bw), sh - (2*c->bw), 0);
			sx += WIDTH(c) + iv;
		}
}

/*
 * nrowgrid layout + gaps
 * https://dwm.suckless.org/patches/nrowgrid/
 */
void
nrowgrid(Monitor *m)
{
	unsigned int n;
	int ri = 0, ci = 0;  /* counters */
	int oh, ov, ih, iv;                         /* vanitygap settings */
	unsigned int cx, cy, cw, ch;                /* client geometry */
	unsigned int uw = 0, uh = 0, uc = 0;        /* utilization trackers */
	unsigned int cols, rows = m->nmaster + 1;
	Client *c;

	/* count clients */
	getgaps(m, &oh, &ov, &ih, &iv, &n);

	/* nothing to do here */
	if (n == 0)
		return;

	/* force 2 clients to always split vertically */
	if (FORCE_VSPLIT && n == 2)
		rows = 1;

	/* never allow empty rows */
	if (n < rows)
		rows = n;

	/* define first row */
	cols = n / rows;
	uc = cols;
	cy = m->wy + oh;
	ch = (m->wh - 2*oh - ih*(rows - 1)) / rows;
	uh = ch;

	for (c = nexttiled(m->clients); c; c = nexttiled(c->next), ci++) {
		if (ci == cols) {
			uw = 0;
			ci = 0;
			ri++;

			/* next row */
			cols = (n - uc) / (rows - ri);
			uc += cols;
			cy = m->wy + oh + uh + ih;
			uh += ch + ih;
		}

		cx = m->wx + ov + uw;
		cw = (m->ww - 2*ov - uw) / (cols - ci);
		uw += cw + iv;

		resize(c, cx, cy, cw - (2*c->bw), ch - (2*c->bw), 0);
	}
}

/*
 * Default tile layout + gaps
 */
static void
tile(Monitor *m)
{
	unsigned int i, n;
	int oh, ov, ih, iv;
	int mx = 0, my = 0, mh = 0, mw = 0;
	int sx = 0, sy = 0, sh = 0, sw = 0;
	float mfacts, sfacts;
	int mrest, srest;
	Client *c;

	getgaps(m, &oh, &ov, &ih, &iv, &n);
	if (n == 0)
		return;

	sx = mx = m->wx + ov;
	sy = my = m->wy + oh;
	mh = m->wh - 2*oh - ih * (MIN(n, m->nmaster) - 1);
	sh = m->wh - 2*oh - ih * (n - m->nmaster - 1);
	sw = mw = m->ww - 2*ov;

	if (m->nmaster && n > m->nmaster) {
		sw = (mw - iv) * (1 - m->mfact);
		mw = mw - iv - sw;
		sx = mx + mw + iv;
	}

	getfacts(m, mh, sh, &mfacts, &sfacts, &mrest, &srest);

	for (i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
		if (i < m->nmaster) {
			resize(c, mx, my, mw - (2*c->bw), mh * (c->cfact / mfacts) + (i < mrest ? 1 : 0) - (2*c->bw), 0);
			my += HEIGHT(c) + ih;
		} else {
			resize(c, sx, sy, sw - (2*c->bw), sh * (c->cfact / sfacts) + ((i - m->nmaster) < srest ? 1 : 0) - (2*c->bw), 0);
			sy += HEIGHT(c) + ih;
		}
}

/* Sends a window to the next/prev tag */
void
shifttag(const Arg *arg)
{
	Arg shifted;
	shifted.ui = selmon->tagset[selmon->seltags];


	if (arg->i > 0)	/* left circular shift */
		shifted.ui = ((shifted.ui << arg->i) | (shifted.ui >> (LENGTH(tags) - arg->i)));
	else		/* right circular shift */
		shifted.ui = (shifted.ui >> (- arg->i) | shifted.ui << (LENGTH(tags) + arg->i));
	tag(&shifted);
}
/* Sends a window to the next/prev tag that has a client, else it moves it to the next/prev one. */
void
shifttagclients(const Arg *arg)
{

	Arg shifted;
	Client *c;
	unsigned int tagmask = 0;
	shifted.ui = selmon->tagset[selmon->seltags];

	for (c = selmon->clients; c; c = c->next)
		if ((c->tags))
			tagmask = tagmask | c->tags;


	if (arg->i > 0)	/* left circular shift */
		do {
			shifted.ui = (shifted.ui << arg->i)
			   | (shifted.ui >> (LENGTH(tags) - arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	else		/* right circular shift */
		do {
			shifted.ui = (shifted.ui >> (- arg->i)
			   | shifted.ui << (LENGTH(tags) + arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	tag(&shifted);
	view(&shifted);
}
/* Navigate to the next/prev tag */
void
shiftview(const Arg *arg)
{
	Arg shifted;
	shifted.ui = selmon->tagset[selmon->seltags];

	if (arg->i > 0)	/* left circular shift */
		shifted.ui = (shifted.ui << arg->i) | (shifted.ui >> (LENGTH(tags) - arg->i));
	else		/* right circular shift */
		shifted.ui = (shifted.ui >> (- arg->i) | shifted.ui << (LENGTH(tags) + arg->i));
	view(&shifted);
}
/* Navigate to the next/prev tag that has a client, else moves it to the next/prev tag */
void
shiftviewclients(const Arg *arg)
{
	Arg shifted;
	Client *c;
	unsigned int tagmask = 0;
	shifted.ui = selmon->tagset[selmon->seltags];

	for (c = selmon->clients; c; c = c->next)
		if ((c->tags))
			tagmask = tagmask | c->tags;


	if (arg->i > 0)	/* left circular shift */
		do {
			shifted.ui = (shifted.ui << arg->i)
			   | (shifted.ui >> (LENGTH(tags) - arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	else		/* right circular shift */
		do {
			shifted.ui = (shifted.ui >> (- arg->i)
			   | shifted.ui << (LENGTH(tags) + arg->i));
		} while (tagmask && !(shifted.ui & tagmask));
	view(&shifted);
}
/* move the current active window to the next/prev tag and view it. More like following the window */
void
shiftboth(const Arg *arg)
{
	Arg shifted;
	shifted.ui = selmon->tagset[selmon->seltags];

	if (arg->i > 0)	/* left circular shift */
		shifted.ui = ((shifted.ui << arg->i) | (shifted.ui >> (LENGTH(tags) - arg->i)));
	else		/* right circular shift */
		shifted.ui = ((shifted.ui >> (- arg->i) | shifted.ui << (LENGTH(tags) + arg->i)));
	tag(&shifted);
	view(&shifted);
}
//helper function for shiftswaptags.
//see: https://github.com/moizifty/DWM-Build/blob/65379c62640788881486401a0d8c79333751b02f/config.h#L48
void
swaptags(const Arg *arg)
{
	Client *c;
	unsigned int newtag = arg->ui & TAGMASK;
	unsigned int curtag = selmon->tagset[selmon->seltags];

	if (newtag == curtag || !curtag || (curtag & (curtag-1)))
		return;

	for (c = selmon->clients; c != NULL; c = c->next) {
		if ((c->tags & newtag) || (c->tags & curtag))
			c->tags ^= curtag ^ newtag;

		if (!c->tags)
			c->tags = newtag;
	}

	//move to the swaped tag
	//selmon->tagset[selmon->seltags] = newtag;

	focus(NULL);
	arrange(selmon);
}
/* swaps "tags" (all the clients) with the next/prev tag. */
void
shiftswaptags(const Arg *arg)
{
	Arg shifted;
	shifted.ui = selmon->tagset[selmon->seltags];

	if (arg->i > 0)	/* left circular shift */
		shifted.ui = ((shifted.ui << arg->i) | (shifted.ui >> (LENGTH(tags) - arg->i)));
	else		/* right circular shift */
		shifted.ui = ((shifted.ui >> (- arg->i) | shifted.ui << (LENGTH(tags) + arg->i)));
	swaptags(&shifted);
	// uncomment if you also want to "go" (view) the tag where the the clients are going
	//view(&shifted);
}

int
dump_tag(yajl_gen gen, const char *name, const int tag_mask)
{
  // clang-format off
  YMAP(
    YSTR("bit_mask"); YINT(tag_mask);
    YSTR("name"); YSTR(name);
  )
  // clang-format on

  return 0;
}

int
dump_tags(yajl_gen gen, const char *tags[], int tags_len)
{
  // clang-format off
  YARR(
    for (int i = 0; i < tags_len; i++)
      dump_tag(gen, tags[i], 1 << i);
  )
  // clang-format on

  return 0;
}

int
dump_client(yajl_gen gen, Client *c)
{
  // clang-format off
  YMAP(
    YSTR("name"); YSTR(c->name);
    YSTR("tags"); YINT(c->tags);
    YSTR("window_id"); YINT(c->win);
    YSTR("monitor_number"); YINT(c->mon->num);

    YSTR("geometry"); YMAP(
      YSTR("current"); YMAP (
        YSTR("x"); YINT(c->x);
        YSTR("y"); YINT(c->y);
        YSTR("width"); YINT(c->w);
        YSTR("height"); YINT(c->h);
      )
      YSTR("old"); YMAP(
        YSTR("x"); YINT(c->oldx);
        YSTR("y"); YINT(c->oldy);
        YSTR("width"); YINT(c->oldw);
        YSTR("height"); YINT(c->oldh);
      )
    )

    YSTR("size_hints"); YMAP(
      YSTR("base"); YMAP(
        YSTR("width"); YINT(c->basew);
        YSTR("height"); YINT(c->baseh);
      )
      YSTR("step"); YMAP(
        YSTR("width"); YINT(c->incw);
        YSTR("height"); YINT(c->inch);
      )
      YSTR("max"); YMAP(
        YSTR("width"); YINT(c->maxw);
        YSTR("height"); YINT(c->maxh);
      )
      YSTR("min"); YMAP(
        YSTR("width"); YINT(c->minw);
        YSTR("height"); YINT(c->minh);
      )
      YSTR("aspect_ratio"); YMAP(
        YSTR("min"); YDOUBLE(c->mina);
        YSTR("max"); YDOUBLE(c->maxa);
      )
    )

    YSTR("border_width"); YMAP(
      YSTR("current"); YINT(c->bw);
      YSTR("old"); YINT(c->oldbw);
    )

    YSTR("states"); YMAP(
      YSTR("is_fixed"); YBOOL(c->isfixed);
      YSTR("is_floating"); YBOOL(c->isfloating);
      YSTR("is_urgent"); YBOOL(c->isurgent);
      YSTR("never_focus"); YBOOL(c->neverfocus);
      YSTR("old_state"); YBOOL(c->oldstate);
      YSTR("is_fullscreen"); YBOOL(c->isfullscreen);
    )
  )
  // clang-format on

  return 0;
}

int
dump_monitor(yajl_gen gen, Monitor *mon, int is_selected)
{
  // clang-format off
  YMAP(
    YSTR("master_factor"); YDOUBLE(mon->mfact);
    YSTR("num_master"); YINT(mon->nmaster);
    YSTR("num"); YINT(mon->num);
    YSTR("is_selected"); YBOOL(is_selected);

    YSTR("monitor_geometry"); YMAP(
      YSTR("x"); YINT(mon->mx);
      YSTR("y"); YINT(mon->my);
      YSTR("width"); YINT(mon->mw);
      YSTR("height"); YINT(mon->mh);
    )

    YSTR("window_geometry"); YMAP(
      YSTR("x"); YINT(mon->wx);
      YSTR("y"); YINT(mon->wy);
      YSTR("width"); YINT(mon->ww);
      YSTR("height"); YINT(mon->wh);
    )

    YSTR("tagset"); YMAP(
      YSTR("current");  YINT(mon->tagset[mon->seltags]);
      YSTR("old"); YINT(mon->tagset[mon->seltags ^ 1]);
    )

    YSTR("tag_state"); dump_tag_state(gen, mon->tagstate);

    YSTR("clients"); YMAP(
      YSTR("selected"); YINT(mon->sel ? mon->sel->win : 0);
      YSTR("stack"); YARR(
        for (Client* c = mon->stack; c; c = c->snext)
          YINT(c->win);
      )
      YSTR("all"); YARR(
        for (Client* c = mon->clients; c; c = c->next)
          YINT(c->win);
      )
    )

    YSTR("layout"); YMAP(
      YSTR("symbol"); YMAP(
        YSTR("current"); YSTR(mon->ltsymbol);
        YSTR("old"); YSTR(mon->lastltsymbol);
      )
      YSTR("address"); YMAP(
        YSTR("current"); YINT((uintptr_t)mon->lt[mon->sellt]);
        YSTR("old"); YINT((uintptr_t)mon->lt[mon->sellt ^ 1]);
      )
    )

    YSTR("bar"); YMAP(
      YSTR("y"); YINT(mon->by);
      YSTR("is_shown"); YBOOL(mon->showbar);
      YSTR("is_top"); YBOOL(mon->topbar);
      YSTR("window_id"); YINT(mon->barwin);
    )
  )
  // clang-format on

  return 0;
}

int
dump_monitors(yajl_gen gen, Monitor *mons, Monitor *selmon)
{
  // clang-format off
  YARR(
    for (Monitor *mon = mons; mon; mon = mon->next) {
      if (mon == selmon)
        dump_monitor(gen, mon, 1);
      else
        dump_monitor(gen, mon, 0);
    }
  )
  // clang-format on

  return 0;
}

int
dump_layouts(yajl_gen gen, const Layout layouts[], const int layouts_len)
{
  // clang-format off
  YARR(
    for (int i = 0; i < layouts_len; i++) {
      YMAP(
        // Check for a NULL pointer. The cycle layouts patch adds an entry at
        // the end of the layouts array with a NULL pointer for the symbol
        YSTR("symbol"); YSTR((layouts[i].symbol ? layouts[i].symbol : ""));
        YSTR("address"); YINT((uintptr_t)(layouts + i));
      )
    }
  )
  // clang-format on

  return 0;
}

int
dump_tag_state(yajl_gen gen, TagState state)
{
  // clang-format off
  YMAP(
    YSTR("selected"); YINT(state.selected);
    YSTR("occupied"); YINT(state.occupied);
    YSTR("urgent"); YINT(state.urgent);
  )
  // clang-format on

  return 0;
}

int
dump_tag_event(yajl_gen gen, int mon_num, TagState old_state,
               TagState new_state)
{
  // clang-format off
  YMAP(
    YSTR("tag_change_event"); YMAP(
      YSTR("monitor_number"); YINT(mon_num);
      YSTR("old_state"); dump_tag_state(gen, old_state);
      YSTR("new_state"); dump_tag_state(gen, new_state);
    )
  )
  // clang-format on

  return 0;
}

int
dump_client_focus_change_event(yajl_gen gen, Client *old_client,
                               Client *new_client, int mon_num)
{
  // clang-format off
  YMAP(
    YSTR("client_focus_change_event"); YMAP(
      YSTR("monitor_number"); YINT(mon_num);
      YSTR("old_win_id"); old_client == NULL ? YNULL() : YINT(old_client->win);
      YSTR("new_win_id"); new_client == NULL ? YNULL() : YINT(new_client->win);
    )
  )
  // clang-format on

  return 0;
}

int
dump_layout_change_event(yajl_gen gen, const int mon_num,
                         const char *old_symbol, const Layout *old_layout,
                         const char *new_symbol, const Layout *new_layout)
{
  // clang-format off
  YMAP(
    YSTR("layout_change_event"); YMAP(
      YSTR("monitor_number"); YINT(mon_num);
      YSTR("old_symbol"); YSTR(old_symbol);
      YSTR("old_address"); YINT((uintptr_t)old_layout);
      YSTR("new_symbol"); YSTR(new_symbol);
      YSTR("new_address"); YINT((uintptr_t)new_layout);
    )
  )
  // clang-format on

  return 0;
}

int
dump_monitor_focus_change_event(yajl_gen gen, const int last_mon_num,
                                const int new_mon_num)
{
  // clang-format off
  YMAP(
    YSTR("monitor_focus_change_event"); YMAP(
      YSTR("old_monitor_number"); YINT(last_mon_num);
      YSTR("new_monitor_number"); YINT(new_mon_num);
    )
  )
  // clang-format on

  return 0;
}

int
dump_focused_title_change_event(yajl_gen gen, const int mon_num,
                                const Window client_id, const char *old_name,
                                const char *new_name)
{
  // clang-format off
  YMAP(
    YSTR("focused_title_change_event"); YMAP(
      YSTR("monitor_number"); YINT(mon_num);
      YSTR("client_window_id"); YINT(client_id);
      YSTR("old_name"); YSTR(old_name);
      YSTR("new_name"); YSTR(new_name);
    )
  )
  // clang-format on

  return 0;
}

int
dump_client_state(yajl_gen gen, const ClientState *state)
{
  // clang-format off
  YMAP(
    YSTR("old_state"); YBOOL(state->oldstate);
    YSTR("is_fixed"); YBOOL(state->isfixed);
    YSTR("is_floating"); YBOOL(state->isfloating);
    YSTR("is_fullscreen"); YBOOL(state->isfullscreen);
    YSTR("is_urgent"); YBOOL(state->isurgent);
    YSTR("never_focus"); YBOOL(state->neverfocus);
  )
  // clang-format on

  return 0;
}

int
dump_focused_state_change_event(yajl_gen gen, const int mon_num,
                                const Window client_id,
                                const ClientState *old_state,
                                const ClientState *new_state)
{
  // clang-format off
  YMAP(
    YSTR("focused_state_change_event"); YMAP(
      YSTR("monitor_number"); YINT(mon_num);
      YSTR("client_window_id"); YINT(client_id);
      YSTR("old_state"); dump_client_state(gen, old_state);
      YSTR("new_state"); dump_client_state(gen, new_state);
    )
  )
  // clang-format on

  return 0;
}

int
dump_error_message(yajl_gen gen, const char *reason)
{
  // clang-format off
  YMAP(
    YSTR("result"); YSTR("error");
    YSTR("reason"); YSTR(reason);
  )
  // clang-format on

  return 0;
}

/**
 * Create IPC socket at specified path and return file descriptor to socket.
 * This initializes the static variable sockaddr.
 */
static int
ipc_create_socket(const char *filename)
{
  char *normal_filename;
  char *parent;
  const size_t addr_size = sizeof(struct sockaddr_un);
  const int sock_type = SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC;

  normalizepath(filename, &normal_filename);

  // In case socket file exists
  unlink(normal_filename);

  // For portability clear the addr structure, since some implementations have
  // nonstandard fields in the structure
  memset(&sockaddr, 0, addr_size);

  parentdir(normal_filename, &parent);
  // Create parent directories
  mkdirp(parent);
  free(parent);

  sockaddr.sun_family = AF_LOCAL;
  strcpy(sockaddr.sun_path, normal_filename);
  free(normal_filename);

  sock_fd = socket(AF_LOCAL, sock_type, 0);
  if (sock_fd == -1) {
    fputs("Failed to create socket\n", stderr);
    return -1;
  }

  DEBUG("Created socket at %s\n", sockaddr.sun_path);

  if (bind(sock_fd, (const struct sockaddr *)&sockaddr, addr_size) == -1) {
    fputs("Failed to bind socket\n", stderr);
    return -1;
  }

  DEBUG("Socket binded\n");

  if (listen(sock_fd, IPC_SOCKET_BACKLOG) < 0) {
    fputs("Failed to listen for connections on socket\n", stderr);
    return -1;
  }

  DEBUG("Now listening for connections on socket\n");

  return sock_fd;
}

/**
 * Internal function used to receive IPC messages from a given file descriptor.
 *
 * Returns -1 on error reading (could be EAGAIN or EINTR)
 * Returns -2 if EOF before header could be read
 * Returns -3 if invalid IPC header
 * Returns -4 if message length exceeds MAX_MESSAGE_SIZE
 */
static int
ipc_recv_message(int fd, uint8_t *msg_type, uint32_t *reply_size,
                 uint8_t **reply)
{
  uint32_t read_bytes = 0;
  const int32_t to_read = sizeof(dwm_ipc_header_t);
  char header[to_read];
  char *walk = header;

  // Try to read header
  while (read_bytes < to_read) {
    const ssize_t n = read(fd, header + read_bytes, to_read - read_bytes);

    if (n == 0) {
      if (read_bytes == 0) {
        fprintf(stderr, "Unexpectedly reached EOF while reading header.");
        fprintf(stderr,
                "Read %" PRIu32 " bytes, expected %" PRIu32 " total bytes.\n",
                read_bytes, to_read);
        return -2;
      } else {
        fprintf(stderr, "Unexpectedly reached EOF while reading header.");
        fprintf(stderr,
                "Read %" PRIu32 " bytes, expected %" PRIu32 " total bytes.\n",
                read_bytes, to_read);
        return -3;
      }
    } else if (n == -1) {
      // errno will still be set
      return -1;
    }

    read_bytes += n;
  }

  // Check if magic string in header matches
  if (memcmp(walk, IPC_MAGIC, IPC_MAGIC_LEN) != 0) {
    fprintf(stderr, "Invalid magic string. Got '%.*s', expected '%s'\n",
            IPC_MAGIC_LEN, walk, IPC_MAGIC);
    return -3;
  }

  walk += IPC_MAGIC_LEN;

  // Extract reply size
  memcpy(reply_size, walk, sizeof(uint32_t));
  walk += sizeof(uint32_t);

  if (*reply_size > MAX_MESSAGE_SIZE) {
    fprintf(stderr, "Message too long: %" PRIu32 " bytes. ", *reply_size);
    fprintf(stderr, "Maximum message size is: %d\n", MAX_MESSAGE_SIZE);
    return -4;
  }

  // Extract message type
  memcpy(msg_type, walk, sizeof(uint8_t));
  walk += sizeof(uint8_t);

  if (*reply_size > 0)
    (*reply) = malloc(*reply_size);
  else
    return 0;

  read_bytes = 0;
  while (read_bytes < *reply_size) {
    const ssize_t n = read(fd, *reply + read_bytes, *reply_size - read_bytes);

    if (n == 0) {
      fprintf(stderr, "Unexpectedly reached EOF while reading payload.");
      fprintf(stderr, "Read %" PRIu32 " bytes, expected %" PRIu32 " bytes.\n",
              read_bytes, *reply_size);
      free(*reply);
      return -2;
    } else if (n == -1) {
      // TODO: Should we return and wait for another epoll event?
      // This would require saving the partial read in some way.
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;

      free(*reply);
      return -1;
    }

    read_bytes += n;
  }

  return 0;
}

/**
 * Internal function used to write a buffer to a file descriptor
 *
 * Returns number of bytes written if successful write
 * Returns 0 if no bytes were written due to EAGAIN or EWOULDBLOCK
 * Returns -1 on unknown error trying to write, errno will carry over from
 *   write() call
 */
static ssize_t
ipc_write_message(int fd, const void *buf, size_t count)
{
  size_t written = 0;

  while (written < count) {
    const ssize_t n = write(fd, (uint8_t *)buf + written, count - written);

    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return written;
      else if (errno == EINTR)
        continue;
      else
        return n;
    }

    written += n;
    DEBUG("Wrote %zu/%zu to client at fd %d\n", written, count, fd);
  }

  return written;
}

/**
 * Initialization for generic event message. This is used to allocate the yajl
 * handle, set yajl options, and in the future any other initialization that
 * should occur for event messages.
 */
static void
ipc_event_init_message(yajl_gen *gen)
{
  *gen = yajl_gen_alloc(NULL);
  yajl_gen_config(*gen, yajl_gen_beautify, 1);
}

/**
 * Prepares buffers of IPC subscribers of specified event using buffer from yajl
 * handle.
 */
static void
ipc_event_prepare_send_message(yajl_gen gen, IPCEvent event)
{
  const unsigned char *buffer;
  size_t len = 0;

  yajl_gen_get_buf(gen, &buffer, &len);
  len++;  // For null char

  for (IPCClient *c = ipc_clients; c; c = c->next) {
    if (c->subscriptions & event) {
      DEBUG("Sending selected client change event to fd %d\n", c->fd);
      ipc_prepare_send_message(c, IPC_TYPE_EVENT, len, (char *)buffer);
    }
  }

  // Not documented, but this frees temp_buffer
  yajl_gen_free(gen);
}

/**
 * Initialization for generic reply message. This is used to allocate the yajl
 * handle, set yajl options, and in the future any other initialization that
 * should occur for reply messages.
 */
static void
ipc_reply_init_message(yajl_gen *gen)
{
  *gen = yajl_gen_alloc(NULL);
  yajl_gen_config(*gen, yajl_gen_beautify, 1);
}

/**
 * Prepares the IPC client's buffer with a message using the buffer of the yajl
 * handle.
 */
static void
ipc_reply_prepare_send_message(yajl_gen gen, IPCClient *c,
                               IPCMessageType msg_type)
{
  const unsigned char *buffer;
  size_t len = 0;

  yajl_gen_get_buf(gen, &buffer, &len);
  len++;  // For null char

  ipc_prepare_send_message(c, msg_type, len, (const char *)buffer);

  // Not documented, but this frees temp_buffer
  yajl_gen_free(gen);
}

/**
 * Find the IPCCommand with the specified name
 *
 * Returns 0 if a command with the specified name was found
 * Returns -1 if a command with the specified name could not be found
 */
static int
ipc_get_ipc_command(const char *name, IPCCommand *ipc_command)
{
  for (int i = 0; i < ipc_commands_len; i++) {
    if (strcmp(ipc_commands[i].name, name) == 0) {
      *ipc_command = ipc_commands[i];
      return 0;
    }
  }

  return -1;
}

/**
 * Parse a IPC_TYPE_RUN_COMMAND message from a client. This function extracts
 * the arguments, argument count, argument types, and command name and returns
 * the parsed information as an IPCParsedCommand. If this function returns
 * successfully, the parsed_command must be freed using
 * ipc_free_parsed_command_members.
 *
 * Returns 0 if the message was successfully parsed
 * Returns -1 otherwise
 */
static int
ipc_parse_run_command(char *msg, IPCParsedCommand *parsed_command)
{
  char error_buffer[1000];
  yajl_val parent = yajl_tree_parse(msg, error_buffer, 1000);

  if (parent == NULL) {
    fputs("Failed to parse command from client\n", stderr);
    fprintf(stderr, "%s\n", error_buffer);
    fprintf(stderr, "Tried to parse: %s\n", msg);
    return -1;
  }

  // Format:
  // {
  //   "command": "<command name>"
  //   "args": [ "arg1", "arg2", ... ]
  // }
  const char *command_path[] = {"command", 0};
  yajl_val command_val = yajl_tree_get(parent, command_path, yajl_t_string);

  if (command_val == NULL) {
    fputs("No command key found in client message\n", stderr);
    yajl_tree_free(parent);
    return -1;
  }

  const char *command_name = YAJL_GET_STRING(command_val);
  size_t command_name_len = strlen(command_name);
  parsed_command->name = (char *)malloc((command_name_len + 1) * sizeof(char));
  strcpy(parsed_command->name, command_name);

  DEBUG("Received command: %s\n", parsed_command->name);

  const char *args_path[] = {"args", 0};
  yajl_val args_val = yajl_tree_get(parent, args_path, yajl_t_array);

  if (args_val == NULL) {
    fputs("No args key found in client message\n", stderr);
    yajl_tree_free(parent);
    return -1;
  }

  unsigned int *argc = &parsed_command->argc;
  Arg **args = &parsed_command->args;
  ArgType **arg_types = &parsed_command->arg_types;

  *argc = args_val->u.array.len;

  // If no arguments are specified, make a dummy argument to pass to the
  // function. This is just the way dwm's void(Arg*) functions are setup.
  if (*argc == 0) {
    *args = (Arg *)malloc(sizeof(Arg));
    *arg_types = (ArgType *)malloc(sizeof(ArgType));
    (*arg_types)[0] = ARG_TYPE_NONE;
    (*args)[0].i = 0;
    (*argc)++;
  } else if (*argc > 0) {
    *args = (Arg *)calloc(*argc, sizeof(Arg));
    *arg_types = (ArgType *)malloc(*argc * sizeof(ArgType));

    for (int i = 0; i < *argc; i++) {
      yajl_val arg_val = args_val->u.array.values[i];

      if (YAJL_IS_NUMBER(arg_val)) {
        if (YAJL_IS_INTEGER(arg_val)) {
          // Any values below 0 must be a signed int
          if (YAJL_GET_INTEGER(arg_val) < 0) {
            (*args)[i].i = YAJL_GET_INTEGER(arg_val);
            (*arg_types)[i] = ARG_TYPE_SINT;
            DEBUG("i=%ld\n", (*args)[i].i);
            // Any values above 0 should be an unsigned int
          } else if (YAJL_GET_INTEGER(arg_val) >= 0) {
            (*args)[i].ui = YAJL_GET_INTEGER(arg_val);
            (*arg_types)[i] = ARG_TYPE_UINT;
            DEBUG("ui=%ld\n", (*args)[i].i);
          }
          // If the number is not an integer, it must be a float
        } else {
          (*args)[i].f = (float)YAJL_GET_DOUBLE(arg_val);
          (*arg_types)[i] = ARG_TYPE_FLOAT;
          DEBUG("f=%f\n", (*args)[i].f);
          // If argument is not a number, it must be a string
        }
      } else if (YAJL_IS_STRING(arg_val)) {
        char *arg_s = YAJL_GET_STRING(arg_val);
        size_t arg_s_size = (strlen(arg_s) + 1) * sizeof(char);
        (*args)[i].v = (char *)malloc(arg_s_size);
        (*arg_types)[i] = ARG_TYPE_STR;
        strcpy((char *)(*args)[i].v, arg_s);
      }
    }
  }

  yajl_tree_free(parent);

  return 0;
}

/**
 * Free the members of a IPCParsedCommand struct
 */
static void
ipc_free_parsed_command_members(IPCParsedCommand *command)
{
  for (int i = 0; i < command->argc; i++) {
    if (command->arg_types[i] == ARG_TYPE_STR) free((void *)command->args[i].v);
  }
  free(command->args);
  free(command->arg_types);
  free(command->name);
}

/**
 * Check if the given arguments are the correct length and type. Also do any
 * casting to correct the types.
 *
 * Returns 0 if the arguments were the correct length and types
 * Returns -1 if the argument count doesn't match
 * Returns -2 if the argument types don't match
 */
static int
ipc_validate_run_command(IPCParsedCommand *parsed, const IPCCommand actual)
{
  if (actual.argc != parsed->argc) return -1;

  for (int i = 0; i < parsed->argc; i++) {
    ArgType ptype = parsed->arg_types[i];
    ArgType atype = actual.arg_types[i];

    if (ptype != atype) {
      if (ptype == ARG_TYPE_UINT && atype == ARG_TYPE_PTR)
        // If this argument is supposed to be a void pointer, cast it
        parsed->args[i].v = (void *)parsed->args[i].ui;
      else if (ptype == ARG_TYPE_UINT && atype == ARG_TYPE_SINT)
        // If this argument is supposed to be a signed int, cast it
        parsed->args[i].i = parsed->args[i].ui;
      else
        return -2;
    }
  }

  return 0;
}

/**
 * Convert event name to their IPCEvent equivalent enum value
 *
 * Returns 0 if a valid event name was given
 * Returns -1 otherwise
 */
static int
ipc_event_stoi(const char *subscription, IPCEvent *event)
{
  if (strcmp(subscription, "tag_change_event") == 0)
    *event = IPC_EVENT_TAG_CHANGE;
  else if (strcmp(subscription, "client_focus_change_event") == 0)
    *event = IPC_EVENT_CLIENT_FOCUS_CHANGE;
  else if (strcmp(subscription, "layout_change_event") == 0)
    *event = IPC_EVENT_LAYOUT_CHANGE;
  else if (strcmp(subscription, "monitor_focus_change_event") == 0)
    *event = IPC_EVENT_MONITOR_FOCUS_CHANGE;
  else if (strcmp(subscription, "focused_title_change_event") == 0)
    *event = IPC_EVENT_FOCUSED_TITLE_CHANGE;
  else if (strcmp(subscription, "focused_state_change_event") == 0)
    *event = IPC_EVENT_FOCUSED_STATE_CHANGE;
  else
    return -1;
  return 0;
}

/**
 * Parse a IPC_TYPE_SUBSCRIBE message from a client. This function extracts the
 * event name and the subscription action from the message.
 *
 * Returns 0 if message was successfully parsed
 * Returns -1 otherwise
 */
static int
ipc_parse_subscribe(const char *msg, IPCSubscriptionAction *subscribe,
                    IPCEvent *event)
{
  char error_buffer[100];
  yajl_val parent = yajl_tree_parse((char *)msg, error_buffer, 100);

  if (parent == NULL) {
    fputs("Failed to parse command from client\n", stderr);
    fprintf(stderr, "%s\n", error_buffer);
    return -1;
  }

  // Format:
  // {
  //   "event": "<event name>"
  //   "action": "<subscribe|unsubscribe>"
  // }
  const char *event_path[] = {"event", 0};
  yajl_val event_val = yajl_tree_get(parent, event_path, yajl_t_string);

  if (event_val == NULL) {
    fputs("No 'event' key found in client message\n", stderr);
    return -1;
  }

  const char *event_str = YAJL_GET_STRING(event_val);
  DEBUG("Received event: %s\n", event_str);

  if (ipc_event_stoi(event_str, event) < 0) return -1;

  const char *action_path[] = {"action", 0};
  yajl_val action_val = yajl_tree_get(parent, action_path, yajl_t_string);

  if (action_val == NULL) {
    fputs("No 'action' key found in client message\n", stderr);
    return -1;
  }

  const char *action = YAJL_GET_STRING(action_val);

  if (strcmp(action, "subscribe") == 0)
    *subscribe = IPC_ACTION_SUBSCRIBE;
  else if (strcmp(action, "unsubscribe") == 0)
    *subscribe = IPC_ACTION_UNSUBSCRIBE;
  else {
    fputs("Invalid action specified for subscription\n", stderr);
    return -1;
  }

  yajl_tree_free(parent);

  return 0;
}

/**
 * Parse an IPC_TYPE_GET_DWM_CLIENT message from a client. This function
 * extracts the window id from the message.
 *
 * Returns 0 if message was successfully parsed
 * Returns -1 otherwise
 */
static int
ipc_parse_get_dwm_client(const char *msg, Window *win)
{
  char error_buffer[100];

  yajl_val parent = yajl_tree_parse(msg, error_buffer, 100);

  if (parent == NULL) {
    fputs("Failed to parse message from client\n", stderr);
    fprintf(stderr, "%s\n", error_buffer);
    return -1;
  }

  // Format:
  // {
  //   "client_window_id": <client window id>
  // }
  const char *win_path[] = {"client_window_id", 0};
  yajl_val win_val = yajl_tree_get(parent, win_path, yajl_t_number);

  if (win_val == NULL) {
    fputs("No client window id found in client message\n", stderr);
    return -1;
  }

  *win = YAJL_GET_INTEGER(win_val);

  yajl_tree_free(parent);

  return 0;
}

/**
 * Called when an IPC_TYPE_RUN_COMMAND message is received from a client. This
 * function parses, executes the given command, and prepares a reply message to
 * the client indicating success/failure.
 *
 * NOTE: There is currently no check for argument validity beyond the number of
 * arguments given and types of arguments. There is also no way to check if the
 * function succeeded based on dwm's void(const Arg*) function types. Pointer
 * arguments can cause crashes if they are not validated in the function itself.
 *
 * Returns 0 if message was successfully parsed
 * Returns -1 on failure parsing message
 */
static int
ipc_run_command(IPCClient *ipc_client, char *msg)
{
  IPCParsedCommand parsed_command;
  IPCCommand ipc_command;

  // Initialize struct
  memset(&parsed_command, 0, sizeof(IPCParsedCommand));

  if (ipc_parse_run_command(msg, &parsed_command) < 0) {
    ipc_prepare_reply_failure(ipc_client, IPC_TYPE_RUN_COMMAND,
                              "Failed to parse run command");
    return -1;
  }

  if (ipc_get_ipc_command(parsed_command.name, &ipc_command) < 0) {
    ipc_prepare_reply_failure(ipc_client, IPC_TYPE_RUN_COMMAND,
                              "Command %s not found", parsed_command.name);
    ipc_free_parsed_command_members(&parsed_command);
    return -1;
  }

  int res = ipc_validate_run_command(&parsed_command, ipc_command);
  if (res < 0) {
    if (res == -1)
      ipc_prepare_reply_failure(ipc_client, IPC_TYPE_RUN_COMMAND,
                                "%u arguments provided, %u expected",
                                parsed_command.argc, ipc_command.argc);
    else if (res == -2)
      ipc_prepare_reply_failure(ipc_client, IPC_TYPE_RUN_COMMAND,
                                "Type mismatch");
    ipc_free_parsed_command_members(&parsed_command);
    return -1;
  }

  if (parsed_command.argc == 1)
    ipc_command.func.single_param(parsed_command.args);
  else if (parsed_command.argc > 1)
    ipc_command.func.array_param(parsed_command.args, parsed_command.argc);

  DEBUG("Called function for command %s\n", parsed_command.name);

  ipc_free_parsed_command_members(&parsed_command);

  ipc_prepare_reply_success(ipc_client, IPC_TYPE_RUN_COMMAND);
  return 0;
}

/**
 * Called when an IPC_TYPE_GET_MONITORS message is received from a client. It
 * prepares a reply with the properties of all of the monitors in JSON.
 */
static void
ipc_get_monitors(IPCClient *c, Monitor *mons, Monitor *selmon)
{
  yajl_gen gen;
  ipc_reply_init_message(&gen);
  dump_monitors(gen, mons, selmon);

  ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_MONITORS);
}

/**
 * Called when an IPC_TYPE_GET_TAGS message is received from a client. It
 * prepares a reply with info about all the tags in JSON.
 */
static void
ipc_get_tags(IPCClient *c, const char *tags[], const int tags_len)
{
  yajl_gen gen;
  ipc_reply_init_message(&gen);

  dump_tags(gen, tags, tags_len);

  ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_TAGS);
}

/**
 * Called when an IPC_TYPE_GET_LAYOUTS message is received from a client. It
 * prepares a reply with a JSON array of available layouts
 */
static void
ipc_get_layouts(IPCClient *c, const Layout layouts[], const int layouts_len)
{
  yajl_gen gen;
  ipc_reply_init_message(&gen);

  dump_layouts(gen, layouts, layouts_len);

  ipc_reply_prepare_send_message(gen, c, IPC_TYPE_GET_LAYOUTS);
}

/**
 * Called when an IPC_TYPE_GET_DWM_CLIENT message is received from a client. It
 * prepares a JSON reply with the properties of the client with the specified
 * window XID.
 *
 * Returns 0 if the message was successfully parsed and if the client with the
 *   specified window XID was found
 * Returns -1 if the message could not be parsed
 */
static int
ipc_get_dwm_client(IPCClient *ipc_client, const char *msg, const Monitor *mons)
{
  Window win;

  if (ipc_parse_get_dwm_client(msg, &win) < 0) return -1;

  // Find client with specified window XID
  for (const Monitor *m = mons; m; m = m->next)
    for (Client *c = m->clients; c; c = c->next)
      if (c->win == win) {
        yajl_gen gen;
        ipc_reply_init_message(&gen);

        dump_client(gen, c);

        ipc_reply_prepare_send_message(gen, ipc_client,
                                       IPC_TYPE_GET_DWM_CLIENT);

        return 0;
      }

  ipc_prepare_reply_failure(ipc_client, IPC_TYPE_GET_DWM_CLIENT,
                            "Client with window id %d not found", win);
  return -1;
}

/**
 * Called when an IPC_TYPE_SUBSCRIBE message is received from a client. It
 * subscribes/unsubscribes the client from the specified event and replies with
 * the result.
 *
 * Returns 0 if the message was successfully parsed.
 * Returns -1 if the message could not be parsed
 */
static int
ipc_subscribe(IPCClient *c, const char *msg)
{
  IPCSubscriptionAction action = IPC_ACTION_SUBSCRIBE;
  IPCEvent event = 0;

  if (ipc_parse_subscribe(msg, &action, &event)) {
    ipc_prepare_reply_failure(c, IPC_TYPE_SUBSCRIBE, "Event does not exist");
    return -1;
  }

  if (action == IPC_ACTION_SUBSCRIBE) {
    DEBUG("Subscribing client on fd %d to %d\n", c->fd, event);
    c->subscriptions |= event;
  } else if (action == IPC_ACTION_UNSUBSCRIBE) {
    DEBUG("Unsubscribing client on fd %d to %d\n", c->fd, event);
    c->subscriptions ^= event;
  } else {
    ipc_prepare_reply_failure(c, IPC_TYPE_SUBSCRIBE,
                              "Invalid subscription action");
    return -1;
  }

  ipc_prepare_reply_success(c, IPC_TYPE_SUBSCRIBE);
  return 0;
}

int
ipc_init(const char *socket_path, const int p_epoll_fd, IPCCommand commands[],
         const int commands_len)
{
  // Initialize struct to 0
  memset(&sock_epoll_event, 0, sizeof(sock_epoll_event));

  int socket_fd = ipc_create_socket(socket_path);
  if (socket_fd < 0) return -1;

  ipc_commands = commands;
  ipc_commands_len = commands_len;

  epoll_fd = p_epoll_fd;

  // Wake up to incoming connection requests
  sock_epoll_event.data.fd = socket_fd;
  sock_epoll_event.events = EPOLLIN;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &sock_epoll_event)) {
    fputs("Failed to add sock file descriptor to epoll", stderr);
    return -1;
  }

  return socket_fd;
}

void
ipc_cleanup()
{
  IPCClient *c = ipc_clients;
  // Free clients and their buffers
  while (c) {
    ipc_drop_client(c);
    c = ipc_clients;
  }

  // Stop waking up for socket events
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fd, &sock_epoll_event);

  // Uninitialize all static variables
  epoll_fd = -1;
  sock_fd = -1;
  ipc_commands = NULL;
  ipc_commands_len = 0;
  memset(&sock_epoll_event, 0, sizeof(struct epoll_event));
  memset(&sockaddr, 0, sizeof(struct sockaddr_un));

  // Delete socket
  unlink(sockaddr.sun_path);

  shutdown(sock_fd, SHUT_RDWR);
  close(sock_fd);
}

int
ipc_get_sock_fd()
{
  return sock_fd;
}

IPCClient *
ipc_get_client(int fd)
{
  return ipc_list_get_client(ipc_clients, fd);
}

int
ipc_is_client_registered(int fd)
{
  return (ipc_get_client(fd) != NULL);
}

int
ipc_accept_client()
{
  int fd = -1;

  struct sockaddr_un client_addr;
  socklen_t len = 0;

  // For portability clear the addr structure, since some implementations
  // have nonstandard fields in the structure
  memset(&client_addr, 0, sizeof(struct sockaddr_un));

  fd = accept(sock_fd, (struct sockaddr *)&client_addr, &len);
  if (fd < 0 && errno != EINTR) {
    fputs("Failed to accept IPC connection from client", stderr);
    return -1;
  }

  if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
    fputs("Failed to set flags on new client fd", stderr);
  }

  IPCClient *nc = ipc_client_new(fd);
  if (nc == NULL) return -1;

  // Wake up to messages from this client
  nc->event.data.fd = fd;
  nc->event.events = EPOLLIN | EPOLLHUP;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &nc->event);

  ipc_list_add_client(&ipc_clients, nc);

  DEBUG("%s%d\n", "New client at fd: ", fd);

  return fd;
}

int
ipc_drop_client(IPCClient *c)
{
  int fd = c->fd;
  shutdown(fd, SHUT_RDWR);
  int res = close(fd);

  if (res == 0) {
    struct epoll_event ev;

    // Stop waking up to messages from this client
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev);
    ipc_list_remove_client(&ipc_clients, c);

    free(c->buffer);
    free(c);

    DEBUG("Successfully removed client on fd %d\n", fd);
  } else if (res < 0 && res != EINTR) {
    fprintf(stderr, "Failed to close fd %d\n", fd);
  }

  return res;
}

int
ipc_read_client(IPCClient *c, IPCMessageType *msg_type, uint32_t *msg_size,
                char **msg)
{
  int fd = c->fd;
  int ret =
      ipc_recv_message(fd, (uint8_t *)msg_type, msg_size, (uint8_t **)msg);

  if (ret < 0) {
    // This will happen if these errors occur while reading header
    if (ret == -1 &&
        (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      return -2;

    fprintf(stderr, "Error reading message: dropping client at fd %d\n", fd);
    ipc_drop_client(c);

    return -1;
  }

  // Make sure receive message is null terminated to avoid parsing issues
  if (*msg_size > 0) {
    size_t len = *msg_size;
    nullterminate(msg, &len);
    *msg_size = len;
  }

  DEBUG("[fd %d] ", fd);
  if (*msg_size > 0)
    DEBUG("Received message: '%.*s' ", *msg_size, *msg);
  else
    DEBUG("Received empty message ");
  DEBUG("Message type: %" PRIu8 " ", (uint8_t)*msg_type);
  DEBUG("Message size: %" PRIu32 "\n", *msg_size);

  return 0;
}

ssize_t
ipc_write_client(IPCClient *c)
{
  const ssize_t n = ipc_write_message(c->fd, c->buffer, c->buffer_size);

  if (n < 0) return n;

  // TODO: Deal with client timeouts

  if (n == c->buffer_size) {
    c->buffer_size = 0;
    free(c->buffer);
    // No dangling pointers!
    c->buffer = NULL;
    // Stop waking up when client is ready to receive messages
    if (c->event.events & EPOLLOUT) {
      c->event.events -= EPOLLOUT;
      epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->fd, &c->event);
    }
    return n;
  }

  // Shift unwritten buffer to beginning of buffer and reallocate
  c->buffer_size -= n;
  memmove(c->buffer, c->buffer + n, c->buffer_size);
  c->buffer = (char *)realloc(c->buffer, c->buffer_size);

  return n;
}

void
ipc_prepare_send_message(IPCClient *c, const IPCMessageType msg_type,
                         const uint32_t msg_size, const char *msg)
{
  dwm_ipc_header_t header = {
      .magic = IPC_MAGIC_ARR, .type = msg_type, .size = msg_size};

  uint32_t header_size = sizeof(dwm_ipc_header_t);
  uint32_t packet_size = header_size + msg_size;

  if (c->buffer == NULL)
    c->buffer = (char *)malloc(c->buffer_size + packet_size);
  else
    c->buffer = (char *)realloc(c->buffer, c->buffer_size + packet_size);

  // Copy header to end of client buffer
  memcpy(c->buffer + c->buffer_size, &header, header_size);
  c->buffer_size += header_size;

  // Copy message to end of client buffer
  memcpy(c->buffer + c->buffer_size, msg, msg_size);
  c->buffer_size += msg_size;

  // Wake up when client is ready to receive messages
  c->event.events |= EPOLLOUT;
  epoll_ctl(epoll_fd, EPOLL_CTL_MOD, c->fd, &c->event);
}

void
ipc_prepare_reply_failure(IPCClient *c, IPCMessageType msg_type,
                          const char *format, ...)
{
  yajl_gen gen;
  va_list args;

  // Get output size
  va_start(args, format);
  size_t len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  char *buffer = (char *)malloc((len + 1) * sizeof(char));

  ipc_reply_init_message(&gen);

  va_start(args, format);
  vsnprintf(buffer, len + 1, format, args);
  va_end(args);
  dump_error_message(gen, buffer);

  ipc_reply_prepare_send_message(gen, c, msg_type);
  fprintf(stderr, "[fd %d] Error: %s\n", c->fd, buffer);

  free(buffer);
}

void
ipc_prepare_reply_success(IPCClient *c, IPCMessageType msg_type)
{
  const char *success_msg = "{\"result\":\"success\"}";
  const size_t msg_len = strlen(success_msg) + 1;  // +1 for null char

  ipc_prepare_send_message(c, msg_type, msg_len, success_msg);
}

void
ipc_tag_change_event(int mon_num, TagState old_state, TagState new_state)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_tag_event(gen, mon_num, old_state, new_state);
  ipc_event_prepare_send_message(gen, IPC_EVENT_TAG_CHANGE);
}

void
ipc_client_focus_change_event(int mon_num, Client *old_client,
                              Client *new_client)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_client_focus_change_event(gen, old_client, new_client, mon_num);
  ipc_event_prepare_send_message(gen, IPC_EVENT_CLIENT_FOCUS_CHANGE);
}

void
ipc_layout_change_event(const int mon_num, const char *old_symbol,
                        const Layout *old_layout, const char *new_symbol,
                        const Layout *new_layout)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_layout_change_event(gen, mon_num, old_symbol, old_layout, new_symbol,
                           new_layout);
  ipc_event_prepare_send_message(gen, IPC_EVENT_LAYOUT_CHANGE);
}

void
ipc_monitor_focus_change_event(const int last_mon_num, const int new_mon_num)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_monitor_focus_change_event(gen, last_mon_num, new_mon_num);
  ipc_event_prepare_send_message(gen, IPC_EVENT_MONITOR_FOCUS_CHANGE);
}

void
ipc_focused_title_change_event(const int mon_num, const Window client_id,
                               const char *old_name, const char *new_name)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_focused_title_change_event(gen, mon_num, client_id, old_name, new_name);
  ipc_event_prepare_send_message(gen, IPC_EVENT_FOCUSED_TITLE_CHANGE);
}

void
ipc_focused_state_change_event(const int mon_num, const Window client_id,
                               const ClientState *old_state,
                               const ClientState *new_state)
{
  yajl_gen gen;
  ipc_event_init_message(&gen);
  dump_focused_state_change_event(gen, mon_num, client_id, old_state,
                                  new_state);
  ipc_event_prepare_send_message(gen, IPC_EVENT_FOCUSED_STATE_CHANGE);
}

void
ipc_send_events(Monitor *mons, Monitor **lastselmon, Monitor *selmon)
{
  for (Monitor *m = mons; m; m = m->next) {
    unsigned int urg = 0, occ = 0, tagset = 0;

    for (Client *c = m->clients; c; c = c->next) {
      occ |= c->tags;

      if (c->isurgent) urg |= c->tags;
    }
    tagset = m->tagset[m->seltags];

    TagState new_state = {.selected = tagset, .occupied = occ, .urgent = urg};

    if (memcmp(&m->tagstate, &new_state, sizeof(TagState)) != 0) {
      ipc_tag_change_event(m->num, m->tagstate, new_state);
      m->tagstate = new_state;
    }

    if (m->lastsel != m->sel) {
      ipc_client_focus_change_event(m->num, m->lastsel, m->sel);
      m->lastsel = m->sel;
    }

    if (strcmp(m->ltsymbol, m->lastltsymbol) != 0 ||
        m->lastlt != m->lt[m->sellt]) {
      ipc_layout_change_event(m->num, m->lastltsymbol, m->lastlt, m->ltsymbol,
                              m->lt[m->sellt]);
      strcpy(m->lastltsymbol, m->ltsymbol);
      m->lastlt = m->lt[m->sellt];
    }

    if (*lastselmon != selmon) {
      if (*lastselmon != NULL)
        ipc_monitor_focus_change_event((*lastselmon)->num, selmon->num);
      *lastselmon = selmon;
    }

    Client *sel = m->sel;
    if (!sel) continue;
    ClientState *o = &m->sel->prevstate;
    ClientState n = {.oldstate = sel->oldstate,
                     .isfixed = sel->isfixed,
                     .isfloating = sel->isfloating,
                     .isfullscreen = sel->isfullscreen,
                     .isurgent = sel->isurgent,
                     .neverfocus = sel->neverfocus};
    if (memcmp(o, &n, sizeof(ClientState)) != 0) {
      ipc_focused_state_change_event(m->num, m->sel->win, o, &n);
      *o = n;
    }
  }
}

int
ipc_handle_client_epoll_event(struct epoll_event *ev, Monitor *mons,
                              Monitor **lastselmon, Monitor *selmon,
                              const char *tags[], const int tags_len,
                              const Layout *layouts, const int layouts_len)
{
  int fd = ev->data.fd;
  IPCClient *c = ipc_get_client(fd);

  if (ev->events & EPOLLHUP) {
    DEBUG("EPOLLHUP received from client at fd %d\n", fd);
    ipc_drop_client(c);
  } else if (ev->events & EPOLLOUT) {
    DEBUG("Sending message to client at fd %d...\n", fd);
    if (c->buffer_size) ipc_write_client(c);
  } else if (ev->events & EPOLLIN) {
    IPCMessageType msg_type = 0;
    uint32_t msg_size = 0;
    char *msg = NULL;

    DEBUG("Received message from fd %d\n", fd);
    if (ipc_read_client(c, &msg_type, &msg_size, &msg) < 0) return -1;

    if (msg_type == IPC_TYPE_GET_MONITORS)
      ipc_get_monitors(c, mons, selmon);
    else if (msg_type == IPC_TYPE_GET_TAGS)
      ipc_get_tags(c, tags, tags_len);
    else if (msg_type == IPC_TYPE_GET_LAYOUTS)
      ipc_get_layouts(c, layouts, layouts_len);
    else if (msg_type == IPC_TYPE_RUN_COMMAND) {
      if (ipc_run_command(c, msg) < 0) return -1;
      ipc_send_events(mons, lastselmon, selmon);
    } else if (msg_type == IPC_TYPE_GET_DWM_CLIENT) {
      if (ipc_get_dwm_client(c, msg, mons) < 0) return -1;
    } else if (msg_type == IPC_TYPE_SUBSCRIBE) {
      if (ipc_subscribe(c, msg) < 0) return -1;
    } else {
      fprintf(stderr, "Invalid message type received from fd %d", fd);
      ipc_prepare_reply_failure(c, msg_type, "Invalid message type: %d",
                                msg_type);
    }
    free(msg);
  } else {
    fprintf(stderr, "Epoll event returned %d from fd %d\n", ev->events, fd);
    return -1;
  }

  return 0;
}

int
ipc_handle_socket_epoll_event(struct epoll_event *ev)
{
  if (!(ev->events & EPOLLIN)) return -1;

  // EPOLLIN means incoming client connection request
  fputs("Received EPOLLIN event on socket\n", stderr);
  int new_fd = ipc_accept_client();

  return new_fd;
}

void
resource_load(XrmDatabase db, char *name, enum resource_type rtype, void *dst)
{
	char *sdst = NULL;
	int *idst = NULL;
	float *fdst = NULL;

	sdst = dst;
	idst = dst;
	fdst = dst;

	char fullname[256];
	char *type;
	XrmValue ret;

	snprintf(fullname, sizeof(fullname), "%s.%s", "dwm", name);
	fullname[sizeof(fullname) - 1] = '\0';

	XrmGetResource(db, fullname, "*", &type, &ret);
	if (!(ret.addr == NULL || strncmp("String", type, 64)))
	{
		switch (rtype) {
		case STRING:
			strcpy(sdst, ret.addr);
			break;
		case INTEGER:
			*idst = strtoul(ret.addr, NULL, 10);
			break;
		case FLOAT:
			*fdst = strtof(ret.addr, NULL);
			break;
		}
	}
}

void
load_xresources(void)
{
	Display *display;
	char *resm;
	XrmDatabase db;
	ResourcePref *p;

	display = XOpenDisplay(NULL);
	resm = XResourceManagerString(display);
	if (!resm)
		return;

	db = XrmGetStringDatabase(resm);
	for (p = resources; p < resources + LENGTH(resources); p++)
		resource_load(db, p->name, p->type, p->dst);
	XCloseDisplay(display);
}

int
main(int argc, char *argv[])
{
	if (argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION);
	else if (argc != 1 && strcmp("-s", argv[1]))
		die("usage: dwm [-v]");
	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display");
	if (!(xcon = XGetXCBConnection(dpy)))
		die("dwm: cannot get xcb connection\n");
	if (argc > 1 && !strcmp("-s", argv[1])) {
		XStoreName(dpy, RootWindow(dpy, DefaultScreen(dpy)), argv[2]);
		XCloseDisplay(dpy);
		return 0;
	}
	checkotherwm();
	XrmInitialize();
	load_xresources();
	setup();
#ifdef __OpenBSD__
	if (pledge("stdio rpath proc exec", NULL) == -1)
		die("pledge");
#endif /* __OpenBSD__ */
	scan();
	runautostart();
	run();
	if(restart) execvp(argv[0], argv);
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}

void
insertclient(Client *item, Client *insertItem, int after) {
	Client *c;
	if (item == NULL || insertItem == NULL || item == insertItem) return;
	detach(insertItem);
	if (!after && selmon->clients == item) {
		attach(insertItem);
		return;
	}
	if (after) {
		c = item;
	} else {
		for (c = selmon->clients; c; c = c->next) { if (c->next == item) break; }
	}
	insertItem->next = c->next;
	c->next = insertItem;
}

void
inplacerotate(const Arg *arg)
{
	if(!selmon->sel || (selmon->sel->isfloating && !arg->f)) return;

	unsigned int selidx = 0, i = 0;
	Client *c = NULL, *stail = NULL, *mhead = NULL, *mtail = NULL, *shead = NULL;

	// Determine positionings for insertclient
	for (c = selmon->clients; c; c = c->next) {
		if (ISVISIBLE(c) && !(c->isfloating)) {
		if (selmon->sel == c) { selidx = i; }
		if (i == selmon->nmaster - 1) { mtail = c; }
		if (i == selmon->nmaster) { shead = c; }
		if (mhead == NULL) { mhead = c; }
		stail = c;
		i++;
		}
	}

	// All clients rotate
	if (arg->i == 2) insertclient(selmon->clients, stail, 0);
	if (arg->i == -2) insertclient(stail, selmon->clients, 1);
	// Stack xor master rotate
	if (arg->i == -1 && selidx >= selmon->nmaster) insertclient(stail, shead, 1);
	if (arg->i == 1 && selidx >= selmon->nmaster) insertclient(shead, stail, 0);
	if (arg->i == -1 && selidx < selmon->nmaster)  insertclient(mtail, mhead, 1);
	if (arg->i == 1 && selidx < selmon->nmaster)  insertclient(mhead, mtail, 0);

	// Restore focus position
	i = 0;
	for (c = selmon->clients; c; c = c->next) {
		if (!ISVISIBLE(c) || (c->isfloating)) continue;
		if (i == selidx) { focus(c); break; }
		i++;
	}
	arrange(selmon);
	focus(c);
}
