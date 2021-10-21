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
// sys_win.h

#include "qcommon.h"
#include <windows.h>
#include "resource.h"
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

//#define DEMO

qboolean s_win95;

int			starttime;
int			ActiveApp;
qboolean	Minimized;

unsigned	sys_msg_time;
unsigned	sys_frame_time;


#define	MAX_NUM_ARGVS	128
int			argc;
char		*argv[MAX_NUM_ARGVS];


qboolean CL_InTimeDemo (void);

extern HWND cl_hwnd;


/*
===============================================================================

SYSTEM IO

===============================================================================
*/


void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	MessageBox (NULL, text, "Error", 0 /* MB_OK */);

	exit (1);
}

void Sys_Quit (void)
{
	timeEndPeriod (1);

	CL_Shutdown ();
	Qcommon_Shutdown ();

	exit (0);
}


void WinError (void)
{
	LPVOID lpMsgBuf;

	FormatMessage (
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError (),
		MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
		);

	// Display the string.
	MessageBox (NULL, lpMsgBuf, "GetLastError", MB_OK | MB_ICONINFORMATION);

	// Free the buffer.
	LocalFree (lpMsgBuf);
}

//================================================================


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof (vinfo);

	if (!GetVersionEx (&vinfo))
		Sys_Error ("Couldn't get OS info");

	if (vinfo.dwMajorVersion < 4)
		Sys_Error ("Quake2 requires windows version 4 or greater");
	if (vinfo.dwPlatformId == VER_PLATFORM_WIN32s)
		Sys_Error ("Quake2 doesn't run on Win32s");
	else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		s_win95 = true;
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
	MSG        msg;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&msg, NULL, 0, 0))
			Sys_Quit ();
		sys_msg_time = msg.time;
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	// grab frame time 
	sys_frame_time = timeGetTime ();	// FIXME: should this be at start?
}



/*
================
Sys_GetClipboardData

================
*/
char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard (NULL) != 0)
	{
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData (CF_TEXT)) != 0)
		{
			if ((cliptext = GlobalLock (hClipboardData)) != 0)
			{
				data = Zone_Alloc (GlobalSize (hClipboardData) + 1);
				strcpy (data, cliptext);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	return data;
}

/*
==============================================================================

WINDOWS CRAP

==============================================================================
*/

/*
=================
Sys_AppActivate
=================
*/
void Sys_AppActivate (void)
{
	ShowWindow (cl_hwnd, SW_RESTORE);
	SetForegroundWindow (cl_hwnd);
}

/*
========================================================================

GAME DLL

========================================================================
*/

static HINSTANCE	game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (!FreeLibrary (game_library))
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void *(*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
#if defined _M_IX86
	const char *gamename = "gamex86.dll";

#ifdef NDEBUG
	const char *debugdir = "release";
#else
	const char *debugdir = "debug";
#endif

#elif defined _M_ALPHA
	const char *gamename = "gameaxp.dll";

#ifdef NDEBUG
	const char *debugdir = "releaseaxp";
#else
	const char *debugdir = "debugaxp";
#endif

#endif

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	_getcwd (cwd, sizeof (cwd));
	Com_sprintf (name, sizeof (name), "%s/%s/%s", cwd, debugdir, gamename);
	game_library = LoadLibrary (name);
	if (game_library)
	{
		Com_DPrintf ("LoadLibrary (%s)\n", name);
	}
	else
	{
		// check the current directory for other development purposes
		Com_sprintf (name, sizeof (name), "%s/%s", cwd, gamename);
		game_library = LoadLibrary (name);
		if (game_library)
		{
			Com_DPrintf ("LoadLibrary (%s)\n", name);
		}
		else
		{
			// now run through the search paths
			path = NULL;
			while (1)
			{
				path = FS_NextPath (path);
				if (!path)
					return NULL;		// couldn't find one anywhere
				Com_sprintf (name, sizeof (name), "%s/%s", path, gamename);
				game_library = LoadLibrary (name);
				if (game_library)
				{
					Com_DPrintf ("LoadLibrary (%s)\n", name);
					break;
				}
			}
		}
	}

	GetGameAPI = (void *) GetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();
		return NULL;
	}

	return GetGameAPI (parms);
}

//=======================================================================


/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	argc = 1;
	argv[0] = "exe";

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}


/*
==================
WinMain

==================
*/

// http://ntcoder.com/bab/tag/getuserprofiledirectory/
#include "userenv.h"
#pragma comment (lib, "userenv.lib")

char *Sys_GetUserHomeDir (void)
{
	static char szHomeDirBuf[MAX_PATH + 1] = {0};

	// We need a process with query permission set
	HANDLE hToken = 0;
	DWORD BufSize = MAX_PATH;

	if (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken))
	{
		// Returns a path like C:/Documents and Settings/nibu if my user name is nibu
		GetUserProfileDirectory (hToken, szHomeDirBuf, &BufSize);

		// Close handle opened via OpenProcessToken
		CloseHandle (hToken);
	}

	return szHomeDirBuf;
}


BOOL Sys_DirectoryExists (LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes (szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


BOOL Sys_FileExists (LPCTSTR szPath)
{
	DWORD dwAttrib = GetFileAttributes (szPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}


void Sys_SetWorkingDirectory (void)
{
	int i, j, k;

	char *homeDir = Sys_GetUserHomeDir ();

	const char *testDirs[] = {
		// these are the locations I have Quake installed to on various PCs; you may want to change it yourself
		homeDir,
		"\\Desktop Crap",
		"\\Games",
		"",
		NULL
	};

	const char *qDirs[] = {
		"Quake II",
		"QuakeII",
		"Quake2",
		"Quake 2",
		"Q II",
		"QII",
		"Q2",
		"Q 2",
		NULL
	};

	const char *qFiles[] = {
		"pak0.pak",
		"gamex86.dll",
		"config.cfg",
		"directq.cfg",
		NULL
	};

	// try to find ID1 content in the test paths
	for (i = 0;; i++)
	{
		if (!testDirs[i]) break;
		if (!Sys_DirectoryExists (testDirs[i])) continue;

		for (j = 0;; j++)
		{
			if (!qDirs[j]) break;
			if (!Sys_DirectoryExists (va ("%s\\%s\\"BASEDIRNAME, testDirs[i], qDirs[j]))) continue;

			for (k = 0;; k++)
			{
				if (!qFiles[k]) break;
				if (!Sys_FileExists (va ("%s\\%s\\"BASEDIRNAME"\\%s", testDirs[i], qDirs[j], qFiles[k]))) continue;

				SetCurrentDirectory (va ("%s\\%s", testDirs[i], qDirs[j]));
				return;
			}
		}
	}
}


int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG				msg;
	int				oldtime;

#ifdef _DEBUG
	// force a working directory for debug builds because the exe isn't going to be in the correct game path
	// as an alternative we could mklink the quake gamedirs to the linker output path as a post-build step
	Sys_SetWorkingDirectory ();
#endif

	ParseCommandLine (lpCmdLine);

	Qcommon_Init (argc, argv);

	// prime the timers
	// note : the intent is clearly that sys_currmsec (formerly called curtime) should only be set once per-frame, then everything can
	// run with the same view of what the current time is.  in practice, with Sys_Milliseconds being called multiple times per frame,
	// curtime was likewise set multiple times and therefore different objects may have disjointed views of time; worse - this can
	// depend on what's going on in the current scene, and if game code triggers a call to Sys_Milliseconds then the disjointed views
	// can be potentially be controlled and exploited by players.  To prevent all of that we go back to setting curtime/sys_currmsec
	// once only at startup, and updating it once only per pass through the main loop.
	oldtime = sys_currmsec = Sys_Milliseconds ();

	// get better sleep time granularity
	timeBeginPeriod (1);

	/* main window message loop */
	while (1)
	{
		// if at a full screen console, don't update unless needed
		if (Minimized)
		{
			Sleep (1);
		}

		while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage (&msg, NULL, 0, 0))
				Com_Quit ();
			sys_msg_time = msg.time;
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		if (!CL_InTimeDemo ())
		{
			// not in a timedemo so ensure that at least 1ms has elapsed before running a frame
			while ((sys_currmsec = Sys_Milliseconds ()) - oldtime < 1)
				Sleep (1);
		}
		else sys_currmsec = Sys_Milliseconds ();

		Qcommon_Frame (sys_currmsec - oldtime);
		oldtime = sys_currmsec;
	}

	// never gets here
	return TRUE;
}
