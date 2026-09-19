/* --------------------------------- sdltimer.c ----------------------------- */

/* This is part of the flight simulator 'fly8'.
 * Author: Eyal Lebedinsky (eyal@eyal.emu.id.au).
*/

/* Time services for SDL2: millisecond ticks and a high resolution counter
 * for the intervals. Based on the SDL timer by Chris Collins (neko-rework).
*/

#include <SDL.h>

#include "fly.h"

#include <time.h>

static Uint64	freq = 1;

LOCAL_FUNC int FAR
stm_init (char *options)
{
	freq = SDL_GetPerformanceFrequency ();
	if (!freq)
		freq = 1;
	return (0);
}

LOCAL_FUNC Ulong FAR
stm_milli (void)
{
	return (SDL_GetTicks ());
}

LOCAL_FUNC int FAR
stm_hires (void)
{
	return ((int)SDL_GetPerformanceCounter ());
}

LOCAL_FUNC char * FAR
stm_ctime (void)
{
	time_t	tm;
	char	*t;

	tm = time (0);
	t = ctime (&tm);
	t[strlen (t) - 1] = '\0';	/* kill NewLine */
	return (t);
}

#define NINTS		10

LOCAL_FUNC Ulong FAR
stm_interval (int mode, Ulong res)
{
	static Uint64	last_time[NINTS];
	static int	n = -1;
	Uint64		t, tt = 0;

	if (mode & TMR_PUSH) {
		++n;
		if (n >= NINTS) {
			LogPrintf ("%s: too many PUSHes... aborting\n",
				Tm->name);
			die ();
		}
	} else if (n < 0) {
		LogPrintf ("%s: too many POPs... aborting\n", Tm->name);
		die ();
	}

	if (mode & (TMR_READ|TMR_SET))
		tt = SDL_GetPerformanceCounter ();

	if (mode & TMR_READ) {
		t = tt - last_time[n];
		t = t * (res ? res : 1000) / freq;
	} else
		t = 0;

	if (mode & TMR_SET)
		last_time[n] = tt;

	if (mode & TMR_POP)
		--n;
	return ((Ulong)t);
}

struct TmDriver TmDriver = {
	"SDL",
	0,
	NULL,	/* extra */
	stm_init,
	0,	/* term */
	stm_milli,
	stm_hires,
	stm_ctime,
	stm_interval
};

#undef NINTS
