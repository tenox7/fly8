/* --------------------------------- grsdl.c -------------------------------- */

/* This is part of the flight simulator 'fly8'.
 * Author: Eyal Lebedinsky (eyal@eyal.emu.id.au).
*/

/* Graphics driver for SDL2. Drawing is done into a byte buffer by bgr.c,
 * then converted through the palette into a streaming texture. Since SDL
 * owns the event queue this driver also feeds the keyboard (console.c)
 * and the mouse (mouse.c) through the GrxExtra hooks.
 *
 * Options: single, hidpi, full, novsync, stats.
*/

#include <SDL.h>

#include "fly.h"
#include "bgr.h"
#include "grx.h"
#include "fly8icon.h"

#define GRS_SINGLE	0x0001
#define GRS_HIDPI	0x0002
#define GRS_FULL	0x0004
#define GRS_NOVSYNC	0x0008
#define GRS_STATS	0x0010
#define INITED		0x8000

#define APPNAME		"Fly8"
#define NPAGES		2

static SDL_Window	*TheWindow = NULL;
static SDL_Renderer	*TheRenderer = NULL;
static SDL_Texture	*TheTexture = NULL;
static char		*ThePages[NPAGES] = {NULL, NULL};
static Uint32		lut[256];
static Uint		_width = 0, _height = 0;
static int		active = 0, visible = 0, dirty = 0;

LOCAL_FUNC void
GrsPresent (int page)
{
	void	*pixels;
	int	pitch;
	Uint	x, y;
	Uchar	*src;
	Uint32	*dst;

	if (!TheTexture || !ThePages[page])
		return;
	if (SDL_LockTexture (TheTexture, NULL, &pixels, &pitch))
		return;
	src = (Uchar *)ThePages[page];
	for (y = 0; y < _height; ++y) {
		dst = (Uint32 *)((char *)pixels + y*pitch);
		for (x = 0; x < _width; ++x)
			dst[x] = lut[src[x]];
		src += _width;
	}
	SDL_UnlockTexture (TheTexture);
	SDL_RenderCopy (TheRenderer, TheTexture, NULL, NULL);
	SDL_RenderPresent (TheRenderer);
	dirty = 0;
}

LOCAL_FUNC void
GrsDestroyImage (void)
{
	int	i;

	for (i = 0; i < NPAGES; ++i) {
		if (ThePages[i])
			free (ThePages[i]);
		ThePages[i] = NULL;
	}
	if (TheTexture)
		SDL_DestroyTexture (TheTexture);
	TheTexture = NULL;
}

LOCAL_FUNC int
GrsCreateImage (Uint width, Uint height)
{
	int	i, npages;

	GrsDestroyImage ();
	_width  = width;
	_height = height;
	npages = (Gr->flags & GRS_SINGLE) ? 1 : NPAGES;
	for (i = 0; i < npages; ++i) {
		if (F(ThePages[i] = (char *)calloc (_width, _height)))
			goto bad;
	}
	TheTexture = SDL_CreateTexture (TheRenderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, _width, _height);
	if (!TheTexture)
		goto bad;
	bSetSize (_width, _height, _width);
	bSetWriteMode (T_MSET);
	bSetActive (ThePages[active]);
	return (0);
bad:
	LogPrintf ("%s: CreateImage failed: %s\n", Gr->name, SDL_GetError ());
	GrsDestroyImage ();
	return (1);
}

LOCAL_FUNC void
GrsSetIcon (void)
{
	SDL_Surface	*s;
	int		w, h, nc, cpp, i, x, y;
	Uint32		colors[256], c;
	Uchar		keys[256];
	unsigned	r, g, b;
	char		*line;

	if (sscanf (fly8_xpm[0], "%d %d %d %d", &w, &h, &nc, &cpp) != 4
	    || cpp != 1 || nc > rangeof (colors))
		return;
	for (i = 0; i < nc; ++i) {
		line = fly8_xpm[1+i];
		keys[i] = line[0];
		if (sscanf (line+1, " c #%4x%4x%4x", &r, &g, &b) != 3)
			return;
		colors[i] = 0xff000000 | (r>>8)<<16 | (g>>8)<<8 | (b>>8);
	}
	s = SDL_CreateRGBSurfaceWithFormat (0, w, h, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (!s)
		return;
	for (y = 0; y < h; ++y) {
		line = fly8_xpm[1+nc+y];
		for (x = 0; x < w; ++x) {
			c = colors[0];
			for (i = 0; i < nc; ++i) {
				if (keys[i] == line[x]) {
					c = colors[i];
					break;
				}
			}
			((Uint32 *)((char *)s->pixels + y*s->pitch))[x] = c;
		}
	}
	SDL_SetWindowIcon (TheWindow, s);
	SDL_FreeSurface (s);
}

LOCAL_FUNC void
GrsMoveTo (Uint x, Uint y)
{
	bMoveTo (x, y);
}

LOCAL_FUNC void
GrsDrawTo (Uint x, Uint y, Uint c)
{
	bDrawTo (x, y, c);
	if (active == visible)
		dirty = 1;
}

LOCAL_FUNC void FAR
GrsEllipse (Uint x, Uint y, Uint rx, Uint ry, Uint c)
{
	bDrawEllipse (x, y, rx, ry, c);
	if (active == visible)
		dirty = 1;
}

LOCAL_FUNC void FAR
GrsPolygon (int npoints, BUFLINE *points, Uint color)
{
	bPolygon (npoints, points, color);
	if (active == visible)
		dirty = 1;
}

LOCAL_FUNC void FAR
GrsClear (Uint x, Uint y, Uint sx, Uint sy, Uint color)
{
	bClear (x, y, sx, sy, color);
	if (active == visible)
		dirty = 1;
}

LOCAL_FUNC int
GrsSetActive (int page)
{
	if (Gr->flags & GRS_SINGLE)
		page = 0;
	active = page;
	bSetActive (ThePages[page]);
	return (0);
}

LOCAL_FUNC int
GrsSetVisual (int page)
{
	if (Gr->flags & GRS_SINGLE)
		page = 0;
	visible = page;
	GrsPresent (page);
	return (0);
}

LOCAL_FUNC int
GrsSetPalette (int n, long c)
{
	if (n < 0 || n >= rangeof (lut))
		return (-1);
	lut[n] = 0xff000000 | (Uint32)C_RGB_R (c) << 16
		| (Uint32)C_RGB_G (c) << 8 | (Uint32)C_RGB_B (c);
	return (n);
}

LOCAL_FUNC void
GrsFlush (void)
{
	if (dirty)
		GrsPresent (visible);
}

LOCAL_FUNC void
GrsResize (void)
{
	int	w, h;

	SDL_GetRendererOutputSize (TheRenderer, &w, &h);
	if ((Uint)w == _width && (Uint)h == _height)
		return;

	sim_set ();
	screen_empty ();
	if (GrsCreateImage (w, h)) {
		LogPrintf ("%s: Resize failed\n", Gr->name);
		die ();
	}
	set_screen (w, h);
	screen_start ();
	sim_reset ();
}

LOCAL_FUNC int
GrsKey (SDL_KeyboardEvent *k)
{
	SDL_Keycode	sym = k->keysym.sym;
	Uint16		mod = k->keysym.mod;
	int		c;

	switch (sym) {
	case SDLK_ESCAPE:
		return (K_ESC);
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		return (K_ENTER);
	case SDLK_BACKSPACE:
		return (K_RUBOUT);
	case SDLK_TAB:
		return (K_TAB);
	case SDLK_DELETE:
		return (K_DEL);
	case SDLK_RIGHT:
		c = K_RIGHT;
		break;
	case SDLK_LEFT:
		c = K_LEFT;
		break;
	case SDLK_UP:
		c = K_UP;
		break;
	case SDLK_DOWN:
		c = K_DOWN;
		break;
	case SDLK_HOME:
		c = K_HOME;
		break;
	case SDLK_END:
		c = K_END;
		break;
	case SDLK_PAGEUP:
		c = K_PGUP;
		break;
	case SDLK_PAGEDOWN:
		c = K_PGDN;
		break;
	case SDLK_INSERT:
		c = K_INS;
		break;
	case SDLK_F1:
	case SDLK_F2:
	case SDLK_F3:
	case SDLK_F4:
	case SDLK_F5:
	case SDLK_F6:
	case SDLK_F7:
	case SDLK_F8:
	case SDLK_F9:
	case SDLK_F10:
	case SDLK_F11:
	case SDLK_F12:
		c = K_F1 + (sym - SDLK_F1);
		break;
	default:
		if (mod & KMOD_GUI)
			return (-1);
		if (mod & KMOD_CTRL) {
			if ((sym >= 'a' && sym <= 'z') || sym == '['
			    || sym == '\\' || sym == ']')
				return (sym | K_CTRL);
			return (-1);
		}
		if ((mod & KMOD_ALT) && sym >= ' ' && sym < 0x7f) {
			if ((mod & KMOD_SHIFT) && sym >= 'a' && sym <= 'z')
				sym -= 'a' - 'A';
			return (sym);
		}
		return (-1);		/* printable: wait for TEXTINPUT */
	}

	c |= K_SPECIAL;
	if (mod & KMOD_SHIFT)
		c |= K_SHIFT;
	if (mod & KMOD_CTRL)
		c |= K_CTRL;
	if (mod & KMOD_ALT)
		c |= K_ALT;
	return (c);
}

LOCAL_FUNC int
GrsText (char *text)
{
	int	c;

	c = 0x0ff & text[0];
	if (c < ' ' || c >= 0x7f)
		return (-1);
	return (c);
}

LOCAL_FUNC void
GrsWindowEvent (SDL_WindowEvent *w)
{
	switch (w->event) {
	case SDL_WINDOWEVENT_SIZE_CHANGED:
		GrsResize ();
		break;
	case SDL_WINDOWEVENT_EXPOSED:
		GrsPresent (visible);
		break;
	default:
		break;
	}
}

LOCAL_FUNC int
GrsKread (void)
{
	SDL_Event	e;
	int		n;

	for (n = -1; n < 0;) {
		if (!SDL_WaitEventTimeout (&e, 1))	/* idle: don't spin */
			return (-1);
		switch (e.type) {
		case SDL_QUIT:
			st.flags1 |= SF_TERM;
			n = 0;
			break;
		case SDL_KEYDOWN:
			n = GrsKey (&e.key);
			break;
		case SDL_TEXTINPUT:
			n = GrsText (e.text.text);
			break;
		case SDL_WINDOWEVENT:
			GrsWindowEvent (&e.window);
			break;
		default:
			break;
		}
	}
	return (n);
}

LOCAL_FUNC int
GrsGetMouse (int *win_x, int *win_y, char *btn, int *nbtn)
{
	int	x, y, w, h;
	Uint32	b;

	b = SDL_GetMouseState (&x, &y);
	SDL_GetWindowSize (TheWindow, &w, &h);
	if (w > 0 && h > 0) {
		x = muldiv (x, _width, w);
		y = muldiv (y, _height, h);
	}
	*win_x = x;
	*win_y = y;
	btn[0] = (char)T(b & SDL_BUTTON_RMASK);
	btn[1] = (char)T(b & SDL_BUTTON_LMASK);
	btn[2] = (char)T(b & SDL_BUTTON_MMASK);
	*nbtn = 3;
	return (0);
}

LOCAL_FUNC void
GrsTerm (DEVICE *dev)
{
	GrsDestroyImage ();
	if (TheRenderer)
		SDL_DestroyRenderer (TheRenderer);
	TheRenderer = NULL;
	if (TheWindow)
		SDL_DestroyWindow (TheWindow);
	TheWindow = NULL;
	SDL_QuitSubSystem (SDL_INIT_VIDEO);

	if (!(Gr->flags & INITED))
		return;
	Gr->flags &= ~INITED;
	if (Gr->flags & GRS_STATS)
		LogStats ();
	LogPrintf ("%s: term ok\n", Gr->name);
}

LOCAL_FUNC int
GrsInit (DEVICE *dev, char *options)
{
	Uint32	wflags, rflags;
	int	i, w, h;

	Gr->flags = 0;
	if (get_parg (options, "single"))
		Gr->flags |= GRS_SINGLE;
	if (get_parg (options, "hidpi"))
		Gr->flags |= GRS_HIDPI;
	if (get_parg (options, "full"))
		Gr->flags |= GRS_FULL;
	if (get_parg (options, "novsync"))
		Gr->flags |= GRS_NOVSYNC;
	if (get_parg (options, "stats"))
		Gr->flags |= GRS_STATS;

	if (dev->sizex == 0 || dev->sizey == 0)
		return (1);

	if (SDL_InitSubSystem (SDL_INIT_VIDEO) < 0) {
		LogPrintf ("%s: SDL init failed: %s\n", Gr->name,
			SDL_GetError ());
		return (1);
	}

	wflags = SDL_WINDOW_RESIZABLE;
	if (Gr->flags & GRS_HIDPI)
		wflags |= SDL_WINDOW_ALLOW_HIGHDPI;
	if (Gr->flags & GRS_FULL)
		wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	TheWindow = SDL_CreateWindow (APPNAME, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, dev->sizex, dev->sizey, wflags);
	if (!TheWindow) {
		LogPrintf ("%s: can't open window: %s\n", Gr->name,
			SDL_GetError ());
		goto bad;
	}
	GrsSetIcon ();

	rflags = SDL_RENDERER_ACCELERATED;
	if (!(Gr->flags & GRS_NOVSYNC))
		rflags |= SDL_RENDERER_PRESENTVSYNC;
	TheRenderer = SDL_CreateRenderer (TheWindow, -1, rflags);
	if (!TheRenderer) {
		LogPrintf ("%s: can't create renderer: %s\n", Gr->name,
			SDL_GetError ());
		goto bad;
	}
	SDL_StartTextInput ();

	for (i = 0; i < rangeof (lut); ++i)
		lut[i] = 0xff000000;
	lut[CC_WHITE] = 0xffffffff;
	st.colors[CC_WHITE] = CC_WHITE;
	st.colors[CC_BLACK] = CC_BLACK;

	SDL_GetRendererOutputSize (TheRenderer, &w, &h);
	dev->lengx = muldiv (dev->lengx, w, dev->sizex);
	dev->lengy = muldiv (dev->lengy, h, dev->sizey);
	dev->sizex = w;
	dev->sizey = h;
	dev->colors = 256;
	dev->npages = (Gr->flags & GRS_SINGLE) ? 1 : NPAGES;

	active = visible = 0;
	if (GrsCreateImage (w, h))
		goto bad;
	GrsSetVisual (0);
	GrsSetActive (0);

	Gr->flags |= INITED;
	LogPrintf ("%s: init ok, %dx%d\n", Gr->name, w, h);
	return (0);
bad:
	GrsTerm (dev);
	return (1);
}

static struct GrxExtra GrsExtra = {
	GrsGetMouse,
	GrsKread
};

struct GrDriver GrSDL = {
	"GrSDL",
	0,
	&GrsExtra,
	0,		/* Devices */
	GrsInit,
	GrsTerm,
	GrsMoveTo,
	GrsDrawTo,
	GrsSetVisual,
	GrsSetActive,
	GrsClear,
	bSetWriteMode,
	GrsSetPalette,
	GrsEllipse,
	GrsPolygon,
	GrsFlush,
	0		/* Shutters */
};

#undef GRS_SINGLE
#undef GRS_HIDPI
#undef GRS_FULL
#undef GRS_NOVSYNC
#undef GRS_STATS
#undef INITED
#undef APPNAME
#undef NPAGES
