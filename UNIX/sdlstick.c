/* --------------------------------- sdlstick.c ----------------------------- */

/* This is part of the flight simulator 'fly8'.
 * Author: Eyal Lebedinsky (eyal@eyal.emu.id.au).
*/

/* Joystick pointer driver for SDL2. All attached joysticks are used; their
 * axes, buttons and hats are numbered consecutively (stick first, then
 * throttle etc. in SDL order), so a HOTAS setup works as one device.
 * Based on the SDL stick driver by Chris Collins (fly8 neko-rework).
 *
 * Options (besides the standard sx= sy= sr= st= and button options):
 *	js=N	use only joystick N (default: all of them)
 *	rdr	read the rudder axis (default channel 2)
 *	ttl	read the throttle axis (default channel 3), 'ttl-' reverses it
 *	hat	report the first hat as buttons 4-7 like the PC driver
 *		(default: hats follow the real buttons)
 *	play=N	dead zone in percent (default 3)
*/

#include <SDL.h>

#include "fly.h"

#if HAVE_SDL

#define PO		p->opt
#define FA1D		PO[IFA1D]
#define FA1F		PO[IFA1F]
#define FA2D		PO[IFA2D]
#define FA2F		PO[IFA2F]
#define FA3D		PO[IFA3D]
#define FA3F		PO[IFA3F]
#define FA4D		PO[IFA4D]
#define FA4F		PO[IFA4F]
#define POPTS		PO[IPFREE+0]
#define FPLAY		PO[IPFREE+1]

#define RUDDER		0x0001
#define THROTTLE	0x0002
#define HAT		0x0004

#define MAXSTICKS	8
#define NSYMM		3		/* centered axes: x y rudder */

static SDL_Joystick	*sticks[MAXSTICKS];
static int		nsticks = 0, naxes = 0, nbuttons = 0, nhats = 0;
static int		offset[NSYMM] = {0, 0, 0};

LOCAL_FUNC int NEAR
SdlRawAxis (int n)
{
	int	i, k;

	for (i = 0; i < nsticks; ++i) {
		k = SDL_JoystickNumAxes (sticks[i]);
		if (n < k)
			return (SDL_JoystickGetAttached (sticks[i])
				? SDL_JoystickGetAxis (sticks[i], n) : 0);
		n -= k;
	}
	return (0);
}

LOCAL_FUNC int NEAR
SdlRawButton (int n)
{
	int	i, k;

	for (i = 0; i < nsticks; ++i) {
		k = SDL_JoystickNumButtons (sticks[i]);
		if (n < k)
			return (SDL_JoystickGetButton (sticks[i], n));
		n -= k;
	}
	return (0);
}

LOCAL_FUNC int NEAR
SdlRawHat (int n)
{
	int	i, k;

	for (i = 0; i < nsticks; ++i) {
		k = SDL_JoystickNumHats (sticks[i]);
		if (n < k)
			return (SDL_JoystickGetHat (sticks[i], n));
		n -= k;
	}
	return (SDL_HAT_CENTERED);
}

/* Centered axis as -100...+100 with a dead zone of 'play' percent.
*/
LOCAL_FUNC int NEAR
SdlSymm (int raw, int play)
{
	int	v, sign;

	v = muldiv (raw, 100, SDL_JOYSTICK_AXIS_MAX);
	if (v > 100)
		v = 100;
	else if (v < -100)
		v = -100;
	sign = (v < 0) ? -1 : 1;
	v *= sign;
	if (v <= play)
		return (0);
	return (sign * muldiv (v - play, 100, 100 - play));
}

/* One directional axis (throttle) as 0...100.
*/
LOCAL_FUNC int NEAR
SdlPos (int raw, int sign, int play)
{
	int	v;

	v = muldiv (raw - SDL_JOYSTICK_AXIS_MIN, 100,
		SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);
	if (sign < 0)
		v = 100 - v;
	if (v <= play)
		v = 0;
	else if (v >= 100 - play)
		v = 100;
	return (v);
}

LOCAL_FUNC int FAR
SdlStickRead (POINTER *p)
{
	char	btn[NBTNS];
	int	i, h, n, base;

	if (!nsticks)
		return (1);
	SDL_JoystickUpdate ();

	p->a[0] = (xshort)(-FA1D * SdlSymm (SdlRawAxis (FA1F) - offset[0],
								FPLAY));
	p->a[1] = (xshort)( FA2D * SdlSymm (SdlRawAxis (FA2F) - offset[1],
								FPLAY));
	if (POPTS & RUDDER)
		p->a[2] = (xshort)(FA3D * SdlSymm (SdlRawAxis (FA3F)
							- offset[2], FPLAY));
	if (POPTS & THROTTLE)
		p->a[3] = (xshort)SdlPos (SdlRawAxis (FA4F), FA4D, FPLAY);

	memset (btn, 0, sizeof (btn));
	n = nbuttons;
	if (n > NBTNS)
		n = NBTNS;
	for (i = 0; i < n; ++i)
		btn[i] = (char)SdlRawButton (i);

	for (i = 0; i < nhats; ++i) {
		h = SdlRawHat (i);
		if ((POPTS & HAT) && 0 == i)
			base = 4;
		else
			base = nbuttons + 4*i;
		if (base + 4 > NBTNS)
			break;
		btn[base+0] = (char)T(h & SDL_HAT_UP);
		btn[base+1] = (char)T(h & SDL_HAT_RIGHT);
		btn[base+2] = (char)T(h & SDL_HAT_DOWN);
		btn[base+3] = (char)T(h & SDL_HAT_LEFT);
		if (base + 4 > n)
			n = base + 4;
	}

	do_btns (p, btn, n);
	return (0);
}

/* Take the current stick position as the center.
*/
LOCAL_FUNC int FAR
SdlStickCenter (POINTER *p)
{
	if (!nsticks)
		return (1);
	SDL_JoystickUpdate ();
	offset[0] = SdlRawAxis (FA1F);
	offset[1] = SdlRawAxis (FA2F);
	offset[2] = (POPTS & RUDDER) ? SdlRawAxis (FA3F) : 0;
	return (0);
}

LOCAL_FUNC int FAR
SdlStickCal (POINTER *p)
{
	memset (offset, 0, sizeof (offset));
	return (0);
}

LOCAL_FUNC void FAR
SdlStickTerm (POINTER *p)
{
	int	i;

	for (i = 0; i < nsticks; ++i) {
		if (sticks[i])
			SDL_JoystickClose (sticks[i]);
		sticks[i] = NULL;
	}
	if (nsticks)
		SDL_QuitSubSystem (SDL_INIT_JOYSTICK);
	nsticks = naxes = nbuttons = nhats = 0;
}

LOCAL_FUNC int FAR
SdlStickInit (POINTER *p, char *options)
{
	long	l;
	int	i, only, n;
	char	*o;

	POPTS = 0;
	if (get_parg (options, "rdr"))
		POPTS |= RUDDER;
	if (T(o = get_parg (options, "ttl"))) {
		POPTS |= THROTTLE;
		if ('-' == *o)
			FA4D = -1;
	}
	if (get_parg (options, "hat"))
		POPTS |= HAT;
	if (get_narg (options, "play=", &l) || l < 0 || l > 50)
		l = 3;
	FPLAY = (int)l;
	if (get_narg (options, "js=", &l))
		only = -1;
	else
		only = (int)l;

	if (SDL_InitSubSystem (SDL_INIT_JOYSTICK) < 0) {
		LogPrintf ("%s: SDL init failed: %s\n", p->name,
			SDL_GetError ());
		return (1);
	}
	SDL_JoystickEventState (SDL_IGNORE);

	nsticks = naxes = nbuttons = nhats = 0;
	n = SDL_NumJoysticks ();
	for (i = 0; i < n && nsticks < MAXSTICKS; ++i) {
		if (only >= 0 && i != only)
			continue;
		if (F(sticks[nsticks] = SDL_JoystickOpen (i))) {
			LogPrintf ("%s: can't open joystick %d: %s\n",
				p->name, i, SDL_GetError ());
			continue;
		}
		LogPrintf ("%s: %d \"%s\": axes %d-%d buttons %d-%d hats %d\n",
			p->name, i, SDL_JoystickName (sticks[nsticks]),
			naxes + 1,
			naxes + SDL_JoystickNumAxes (sticks[nsticks]),
			nbuttons,
			nbuttons + SDL_JoystickNumButtons (sticks[nsticks]) - 1,
			SDL_JoystickNumHats (sticks[nsticks]));
		naxes    += SDL_JoystickNumAxes (sticks[nsticks]);
		nbuttons += SDL_JoystickNumButtons (sticks[nsticks]);
		nhats    += SDL_JoystickNumHats (sticks[nsticks]);
		++nsticks;
	}
	if (!nsticks) {
		LogPrintf ("%s: no joystick found\n", p->name);
		SDL_QuitSubSystem (SDL_INIT_JOYSTICK);
		return (1);
	}

	if (FA1F >= naxes || FA2F >= naxes) {
		LogPrintf ("%s: need x/y axes %d/%d, have %d\n", p->name,
			FA1F+1, FA2F+1, naxes);
		SdlStickTerm (p);
		return (1);
	}
	if ((POPTS & RUDDER) && FA3F >= naxes) {
		MsgEPrintf (-50, "%s: no rudder axis %d", p->name, FA3F+1);
		POPTS &= ~RUDDER;
	}
	if ((POPTS & THROTTLE) && FA4F >= naxes) {
		MsgEPrintf (-50, "%s: no throttle axis %d", p->name, FA4F+1);
		POPTS &= ~THROTTLE;
	}
	if ((POPTS & HAT) && !nhats) {
		MsgEPrintf (-50, "%s: no hat", p->name);
		POPTS &= ~HAT;
	}
	SdlStickCal (p);

	LogPrintf ("%s: x=%d y=%d%s%s%s play=%d\n", p->name, FA1F+1, FA2F+1,
		(POPTS & RUDDER) ? " rudder" : "",
		(POPTS & THROTTLE) ? " throttle" : "",
		(POPTS & HAT) ? " hat" : "", FPLAY);
	return (0);
}

struct PtrDriver NEAR PtrSdlStick = {
	"SDLSTICK",
	0,
	NULL,	/* extra */
	SdlStickInit,
	SdlStickTerm,
	SdlStickCal,
	SdlStickCenter,
	SdlStickRead,
	std_key
};

#undef PO
#undef FA1D
#undef FA1F
#undef FA2D
#undef FA2F
#undef FA3D
#undef FA3F
#undef FA4D
#undef FA4F
#undef POPTS
#undef FPLAY
#undef RUDDER
#undef THROTTLE
#undef HAT
#undef MAXSTICKS
#undef NSYMM
#endif /* if HAVE_SDL */
