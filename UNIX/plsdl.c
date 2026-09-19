/* --------------------------------- plsdl.c -------------------------------- */

/* This is part of the flight simulator 'fly8'.
 * Author: Eyal Lebedinsky (eyal@eyal.emu.id.au).
*/

/* Sound device driver for SDL2 audio: a square wave beeper driven by the
 * generic play-list code in plsound.c.
 *
 * Options: volume= (0-100, default 50)
*/

#include <SDL.h>

#include "fly.h"

#if HAVE_SDL

#include "plsound.h"


#define INITED		0x8000
#define RATE		44100

static SDL_AudioDeviceID	TheDevice = 0;
static SDL_atomic_t		freq;
static Sint16			amp = 0;

LOCAL_FUNC void SDLCALL
SdlFill (void *udata, Uint8 *stream, int len)
{
	static Uint32	phase = 0;
	Sint16		*out = (Sint16 *)stream;
	int		n = len / sizeof (*out);
	int		f = SDL_AtomicGet (&freq);
	Uint32		step;

	if (f <= 0) {
		memset (stream, 0, len);
		phase = 0;
		return;
	}
	step = (Uint32)(((Uint64)f << 32) / RATE);
	while (n-- > 0) {
		*out++ = (phase & 0x80000000UL) ? amp : -amp;
		phase += step;
	}
}

LOCAL_FUNC void FAR
SdlStart (int n)
{
	SDL_AtomicSet (&freq, n);
}

LOCAL_FUNC void FAR
SdlStop (void)
{
	SDL_AtomicSet (&freq, 0);
}

LOCAL_FUNC void FAR
SdlTerm (void)
{
	if (TheDevice) {
		SDL_CloseAudioDevice (TheDevice);
		TheDevice = 0;
		SDL_QuitSubSystem (SDL_INIT_AUDIO);
	}
	if (Snd->flags & INITED) {
		PlsTerm ();
		Snd->flags &= ~INITED;
		LogPrintf ("%s: term ok\n", Snd->name);
	}
}

LOCAL_FUNC int FAR
SdlInit (char *options)
{
	SDL_AudioSpec	want, have;
	long		l;

	Snd->flags &= ~INITED;

	if (get_narg (options, "volume=", &l) || l > 100)
		l = 50;
	amp = (Sint16)(l * 80);

	if (SDL_InitSubSystem (SDL_INIT_AUDIO) < 0) {
		LogPrintf ("%s: SDL init failed: %s\n", Snd->name,
			SDL_GetError ());
		return (1);
	}
	memset (&want, 0, sizeof (want));
	want.freq = RATE;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 512;
	want.callback = SdlFill;
	TheDevice = SDL_OpenAudioDevice (NULL, 0, &want, &have, 0);
	if (!TheDevice) {
		LogPrintf ("%s: can't open audio: %s\n", Snd->name,
			SDL_GetError ());
		goto badret;
	}
	SDL_AtomicSet (&freq, 0);
	SDL_PauseAudioDevice (TheDevice, 0);

	if (PlsInit (options))
		goto badret;

	Snd->flags |= INITED;
	LogPrintf ("%s: init ok, volume %ld\n", Snd->name, l);
	return (0);

badret:
	SdlTerm ();
	return (1);
}


static struct plsextra FAR SdlExtra = {
	SdlStart,
	SdlStop,
	NULL,		/* beeps */
	0L,		/* lasttime */
	0L,		/* nexttime */
	0,		/* playing */
	0		/* nbeeps */
};

struct SndDriver NEAR SndPlSDL = {
	"PlSDL",
	0,
	&SdlExtra,
	SdlInit,
	SdlTerm,
	PlsPoll,
	PlsBeep,
	PlsEffect,
	PlsList
};
#endif /* if HAVE_SDL */
