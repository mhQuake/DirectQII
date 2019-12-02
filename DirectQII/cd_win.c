/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include <windows.h>
#include "client.h"

// fully-featured MCI-based MP3 replacement for CDAudio
#define Q_MCI_DEVICE "Q_MCI_DEVICE"


extern	HWND	cl_hwnd;

static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean	enabled = false;
static qboolean playLooping = false;

static byte remap[100];

static byte	playTrack;
static byte	maxTrack;

cvar_t *cd_loopcount;
cvar_t *cd_looptrack;
cvar_t *bgmvolume;

UINT	wDeviceID;
int		loopcounter;

char	cd_basedir[MAX_OSPATH];
char	cd_ext[4];


static qboolean CDAudio_GetTracks (char *basedir, char *ext)
{
	int i;
	FILE *f;

	// start from scratch
	maxTrack = 0;
	cd_basedir[0] = 0;
	cd_ext[0] = 0;

	// (track numbers begin at 2 in Quake)
	for (i = 2;; i++)
	{
		if ((f = fopen (va ("%s/music/track%02i.%s", basedir, i, ext), "rb")) != NULL)
		{
			maxTrack = i;
			fclose (f);
		}
		else break;
	}

	if (maxTrack > 0)
	{
		// store out what we got - all future attempts to play music will look in this path and try this extension
		strcpy (cd_basedir, basedir);
		strcpy (cd_ext, ext);
		return true;
	}
	else return false;
}


static void CDAudio_GetAudioDiskInfo (void)
{
	// try the current gamedir first
	if (CDAudio_GetTracks (FS_Gamedir (), "mp3")) return;
	if (CDAudio_GetTracks (FS_Gamedir (), "ogg")) return;
	if (CDAudio_GetTracks (FS_Gamedir (), "wma")) return;

	// fall back to baseq2 - this may casue baseq2 to be scanned twice, but that's OK
	// (note - because FS_Gamedir is an absolute path whereas BASEDIRNAME is a relative path, there is no 100% robust way of skipping this)
	if (CDAudio_GetTracks (BASEDIRNAME, "mp3")) return;
	if (CDAudio_GetTracks (BASEDIRNAME, "ogg")) return;
	if (CDAudio_GetTracks (BASEDIRNAME, "wma")) return;
}


qboolean CDAudio_SetVolume (qboolean force)
{
	static int lastvolume = -1;
	int s_volume = (int) (bgmvolume->value * 1000.0f);

	if (s_volume < 0) s_volume = 0;
	if (s_volume > 1000) s_volume = 1000;

	if (lastvolume != s_volume || force)
	{
		// yayy - linear scale!
		if (!mciSendString (va ("setaudio "Q_MCI_DEVICE" volume to %i", s_volume), NULL, 0, 0))
			lastvolume = s_volume;
		else return false;
	}

	// succeeded or not needed
	return true;
}


qboolean CDAudio_TryPlay (char *track, qboolean looping)
{
	// 0 if successful - needs filename in quotes as it may contain spaces
	if (!mciSendString (va ("open \"%s\" type mpegvideo alias "Q_MCI_DEVICE, track), NULL, 0, 0))
	{
		// do an initial volume set (forcing it so it will override the cached vol)
		if (CDAudio_SetVolume (true))
		{
			// and attempt to play it
			if (!mciSendString ("set "Q_MCI_DEVICE" video off", NULL, 0, 0))	// in case the file contains video info
			{
				if (!mciSendString ("play "Q_MCI_DEVICE" notify", NULL, 0, cl_hwnd))
				{
					// got it; retrieve the device id for the CDAudio_MessageHandler proc
					if ((wDeviceID = mciGetDeviceID (Q_MCI_DEVICE)) == 0)
					{
						// if we can't get a device ID then we assume something else went wrong and just kill it
						Com_Printf ("failed to get device ID for track %i\n", track);
						CDAudio_Stop ();
						return false;
					}

					// NOW it's OK
					return true;
				}
			}
		}

		// if it gets to here it failed to open, set volume, play, or respond to other controls; most likely a codec is not installed
		Com_Printf ("failed to play track %i - have you a codec for \".%s\" installed?\n", track, cd_ext);

		// prevent all further music
		cd_ext[0] = 0;
	}

	// it shouldn't be playing if we get to here....
	return false;
}


void CDAudio_Play2 (int track, qboolean looping)
{
	// if the same track is requested, and if it's currently playing, just continue it
	if (playing && playTrack == track) return;

	// stop whatever is currently playing
	CDAudio_Stop ();

	// if any of these are NULL we didn't get any tracks
	if (!cd_basedir[0]) return;
	if (!cd_ext[0]) return;
	if (!maxTrack) return;

	// get the track from the remap
	track = remap[track];

	// if it's out of range don't even try (some maps send track 0 which we interpret as an explicit request for no music) (it was already stopped above so it doesn't need to be again)
	if (track < 1 || track > maxTrack) return;

	// try it
	if (!CDAudio_TryPlay (va ("%s/music/track%02i.%s", cd_basedir, track, cd_ext), looping))
		return;

	// we got something so store them out
	playLooping = looping;
	playTrack = track;
	playing = true;
}


void CDAudio_Play (int track, qboolean looping)
{
	// set a loop counter so that this track will change to the - looptrack later
	loopcounter = 0;
	CDAudio_Play2 (track, looping);
}


void CDAudio_Stop (void)
{
	if (!enabled) return;
	if (!playing) return;

	mciSendString ("stop "Q_MCI_DEVICE, NULL, 0, 0);
	mciSendString ("close "Q_MCI_DEVICE, NULL, 0, 0);
	wDeviceID = 0; // device is not open

	wasPlaying = false;
	playing = false;
}


void CDAudio_Pause (void)
{
	if (!enabled) return;
	if (!playing) return;

	mciSendString ("pause "Q_MCI_DEVICE, NULL, 0, 0);

	wasPlaying = playing;
	playing = false;
}


void CDAudio_Resume (void)
{
	if (!enabled) return;
	if (!wasPlaying) return;

	mciSendString ("resume "Q_MCI_DEVICE, NULL, 0, 0);

	playing = true;
}


static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc () < 2)
		return;

	command = Cmd_Argv (1);

	if (Q_strcasecmp (command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (Q_strcasecmp (command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop ();

		enabled = false;
		return;
	}

	if (Q_strcasecmp (command, "reset") == 0)
	{
		enabled = true;

		if (playing)
			CDAudio_Stop ();

		for (n = 0; n < 100; n++)
			remap[n] = n;

		CDAudio_GetAudioDiskInfo ();
		return;
	}

	if (Q_strcasecmp (command, "remap") == 0)
	{
		ret = Cmd_Argc () - 2;

		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Com_Printf ("  %u -> %u\n", n, remap[n]);

			return;
		}

		for (n = 1; n <= ret; n++)
			remap[n] = atoi (Cmd_Argv (n + 1));

		return;
	}

	if (Q_strcasecmp (command, "play") == 0)
	{
		CDAudio_Play (atoi (Cmd_Argv (2)), false);
		return;
	}

	if (Q_strcasecmp (command, "loop") == 0)
	{
		CDAudio_Play (atoi (Cmd_Argv (2)), true);
		return;
	}

	if (Q_strcasecmp (command, "stop") == 0)
	{
		CDAudio_Stop ();
		return;
	}

	if (Q_strcasecmp (command, "pause") == 0)
	{
		CDAudio_Pause ();
		return;
	}

	if (Q_strcasecmp (command, "resume") == 0)
	{
		CDAudio_Resume ();
		return;
	}

	if (Q_strcasecmp (command, "info") == 0)
	{
		Com_Printf ("%u tracks\n", maxTrack);

		if (playing)
			Com_Printf ("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Com_Printf ("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);

		return;
	}
}


LONG CDAudio_MessageHandler (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (lParam != wDeviceID)
		return 1;

	switch (wParam)
	{
	case MCI_NOTIFY_SUCCESSFUL:
		if (playing)
		{
			CDAudio_Stop (); // must be done here because of setting playing = false below
			playing = false; // do this so that the playTrack == track test won't trigger, because we're restarting the same track

			if (playLooping)
			{
				// if the track has played the given number of times,
				// go to the ambient track
				if (++loopcounter >= cd_loopcount->value)
					CDAudio_Play2 (cd_looptrack->value, true);
				else CDAudio_Play2 (playTrack, true);
			}
		}

		break;

	case MCI_NOTIFY_ABORTED:
		Com_Printf ("MCI_NOTIFY_ABORTED\n");
		break;

	case MCI_NOTIFY_SUPERSEDED:
		Com_Printf ("MCI_NOTIFY_SUPERSEDED\n");
		break;

	case MCI_NOTIFY_FAILURE:
		Com_Printf ("MCI_NOTIFY_FAILURE\n");
		CDAudio_Stop ();
		break;

	default:
		Com_Printf ("Unexpected MM_MCINOTIFY type (%i)\n", wParam);
		return 1;
	}

	return 0;
}


void CDAudio_Update (void)
{
	if (playing)
		CDAudio_SetVolume (false);
}


int CDAudio_Init (void)
{
	int n;

	cd_loopcount = Cvar_Get ("cd_loopcount", "4", 0, NULL);
	cd_looptrack = Cvar_Get ("cd_looptrack", "11", 0, NULL);
	bgmvolume = Cvar_Get ("bgmvolume", "0.7", CVAR_ARCHIVE, NULL);

	for (n = 0; n < 100; n++)
		remap[n] = n;

	enabled = true;

	CDAudio_GetAudioDiskInfo ();
	Cmd_AddCommand ("cd", CD_f);

	Com_Printf ("Music Initialized\n");

	return 0;
}


void CDAudio_Shutdown (void)
{
	CDAudio_Stop ();
}


/*
===========
CDAudio_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void CDAudio_Activate (qboolean active)
{
	if (active)
		CDAudio_Resume ();
	else CDAudio_Pause ();
}


